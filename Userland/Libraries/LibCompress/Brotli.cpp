/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/MemoryStream.h>
#include <LibCompress/Brotli.h>
#include <LibCompress/BrotliTables.h>

namespace Compress {

BrotliDecompressor::BrotliDecompressor(InputStream& stream)
    : m_input_stream(stream)
{
}

BrotliDecompressor::~BrotliDecompressor()
{
}

BrotliDecompressor::UncompressedBlock::UncompressedBlock(BrotliDecompressor& decompressor, size_t length)
    : m_decompressor(decompressor)
    , m_bytes_remaining(length)
{
}

bool BrotliDecompressor::UncompressedBlock::try_read_more()
{
    if (m_bytes_remaining == 0)
        return false;

    const auto nread = min(m_bytes_remaining, m_decompressor.m_sliding_window->remaining_contigous_space());
    m_bytes_remaining -= nread;

    m_decompressor.m_input_stream >> m_decompressor.m_sliding_window->reserve_contigous_space(nread);

    return true;
}

size_t BrotliDecompressor::read(Bytes bytes)
{
    auto align_and_verify_padding = [this]() {
        if (m_input_stream.bit_offset() == 0)
            return true;
        auto padding = m_input_stream.read_bits(8 - m_input_stream.bit_offset());
        if (padding == 0)
            return true;
        set_fatal_error();
        return false;
    };

    size_t total_read = 0;
    while (total_read < bytes.size()) {
        if (has_any_error())
            break;

        auto slice = bytes.slice(total_read);

        // https://datatracker.ietf.org/doc/html/rfc7932#section-9.1
        if (m_state == State::ReadingStreamHeader) {
            u8 window_bits;
            if (auto bits = m_input_stream.read_bits(1); bits == 0) { // 0: 16
                window_bits = 16;
            } else if (bits = m_input_stream.read_bits(3); bits != 0) { // 0011: 18, 0101: 19, 0111: 20, 1001: 21, 1011: 22, 1101: 23, 1111: 24
                window_bits = 17 + bits;
            } else if (bits = m_input_stream.read_bits(3); bits != 0) { // 0100001: 10, 0110001: 11, 1000001: 12, 1010001: 13, 1100001: 14, 1110001: 15
                if (bits == 1) {
                    // 0010001: Large Window Size extension, currently unsupported
                    set_fatal_error();
                    break;
                }
                window_bits = 8 + bits;
            } else { // 0000001: 17
                window_bits = 17;
            }
            auto window_size = 1u << window_bits;
            m_sliding_window = HeapBackedCircularDuplexStream(window_size);
            m_max_backwards_reference = window_size - 16;
            m_state = State::ReadingBlockHeader;
        }

        if (m_state == State::ReadingBlockHeader) {
            if (m_read_final_bock) {
                align_and_verify_padding();
                break;
            }

            m_read_final_bock = m_input_stream.read_bit();        // 1 bit: ISLAST
            if (m_read_final_bock && m_input_stream.read_bit()) { // 1 bit: ISLASTEMPTY, only present if ISLAST bit is set
                align_and_verify_padding();
                break;
            }

            auto nibbles_code = m_input_stream.read_bits(2); // 2 bits: MNIBBLES
            if (nibbles_code == 0) {                         // Empty meta block
                if (m_input_stream.read_bit()) {             // 1 bit: reserved, must be zero
                    set_fatal_error();
                    break;
                }
                auto skip_length_bytes = m_input_stream.read_bits(2); // 2 bits: MSKIPBYTES
                u32 skip_length = 0;
                if (skip_length_bytes > 0) { // MSKIPBYTES * 8 bits: MSKIPLEN - 1, present if MSKIPBYTES is positive; otherwise, MSKIPLEN is 0
                    skip_length = m_input_stream.read_bits(skip_length_bytes * 8);
                    if (skip_length_bytes > 1 && (sizeof(skip_length) * 8) - __builtin_clzl(skip_length) <= (skip_length_bytes - 1) * 8) {
                        set_fatal_error();
                        break;
                    }
                    skip_length++;
                }
                if (!align_and_verify_padding())
                    break;
                m_input_stream.discard_or_error(skip_length);
                m_state = State::ReadingBlockHeader;
                continue;
            }

            auto nibbles = 4 + nibbles_code;
            u32 length = m_input_stream.read_bits(nibbles * 4); // MNIBBLES * 4 bits: MLEN - 1
            if (nibbles > 4 && (sizeof(length) * 8) - __builtin_clzl(length) <= (nibbles - 1) * 4) {
                set_fatal_error();
                break;
            }
            length++;

            if (!m_read_final_bock && m_input_stream.read_bit()) { // 1 bit: ISUNCOMPRESSED, only present if the ISLAST bit is not set
                if (!align_and_verify_padding())
                    break;
                m_state = State::ReadingUncompressedBlock;
                new (&m_uncompressed_block) UncompressedBlock(*this, length);
                continue;
            }

            set_fatal_error();
            break;
        }

        if (m_state == State::ReadingCompressedBlock) {
            auto nread = 0u;
            while (nread < slice.size() && m_compressed_block.try_read_more()) {
                nread += m_sliding_window->read(slice.slice(nread));
            }

            if (m_input_stream.has_any_error()) {
                set_fatal_error();
                break;
            }

            total_read += nread;
            if (nread == slice.size())
                break;

            m_compressed_block.~CompressedBlock();
            m_state = State::ReadingBlockHeader;

            continue;
        }

        if (m_state == State::ReadingUncompressedBlock) {
            auto nread = 0u;
            while (nread < slice.size() && m_uncompressed_block.try_read_more()) {
                nread += m_sliding_window->read(slice.slice(nread));
            }

            if (m_input_stream.has_any_error()) {
                set_fatal_error();
                break;
            }

            total_read += nread;
            if (nread == slice.size())
                break;

            m_uncompressed_block.~UncompressedBlock();
            m_state = State::ReadingBlockHeader;

            continue;
        }

        VERIFY_NOT_REACHED();
    }
    return total_read;
}

bool BrotliDecompressor::read_or_error(Bytes bytes)
{
    if (read(bytes) < bytes.size()) {
        set_fatal_error();
        return false;
    }

    return true;
}

bool BrotliDecompressor::discard_or_error(size_t count)
{
    u8 buffer[4096];

    size_t ndiscarded = 0;
    while (ndiscarded < count) {
        if (unreliable_eof()) {
            set_fatal_error();
            return false;
        }

        ndiscarded += read({ buffer, min<size_t>(count - ndiscarded, 4096) });
    }

    return true;
}

bool BrotliDecompressor::unreliable_eof() const { return m_state == State::ReadingBlockHeader && m_read_final_bock; }

bool BrotliDecompressor::handle_any_error()
{
    bool handled_errors = m_input_stream.handle_any_error();
    return Stream::handle_any_error() || handled_errors;
}

Optional<ByteBuffer> BrotliDecompressor::decompress_all(ReadonlyBytes bytes)
{
    InputMemoryStream memory_stream { bytes };
    BrotliDecompressor deflate_stream { memory_stream };
    DuplexMemoryStream output_stream;

    u8 buffer[4096];
    while (!deflate_stream.has_any_error() && !deflate_stream.unreliable_eof()) {
        const auto nread = deflate_stream.read({ buffer, sizeof(buffer) });
        output_stream.write_or_error({ buffer, nread });
    }

    if (deflate_stream.handle_any_error())
        return {};

    return output_stream.copy_into_contiguous_buffer();
}

}
