/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/BitStream.h>
#include <AK/HeapBackedCircularDuplexStream.h>
#include <AK/Optional.h>
#include <AK/Stream.h>
#include <LibCompress/Huffman.h>

namespace Compress {

class BrotliDecompressor final : public InputStream {
    class CompressedBlock {
    public:
        CompressedBlock(BrotliDecompressor&, CanonicalCode literal_codes, Optional<CanonicalCode> distance_codes);

        bool try_read_more();

    private:
        bool m_eof { false };

        BrotliDecompressor& m_decompressor;
        CanonicalCode m_literal_codes;
        Optional<CanonicalCode> m_distance_codes;
    };

    class UncompressedBlock {
    public:
        UncompressedBlock(BrotliDecompressor&, size_t);

        bool try_read_more();

    private:
        BrotliDecompressor& m_decompressor;
        size_t m_bytes_remaining;
    };

    enum class State {
        ReadingStreamHeader,
        ReadingBlockHeader,
        ReadingCompressedBlock,
        ReadingUncompressedBlock
    };

public:
    friend CompressedBlock;
    friend UncompressedBlock;

    BrotliDecompressor(InputStream&);
    ~BrotliDecompressor();

    size_t read(Bytes) override;
    bool read_or_error(Bytes) override;
    bool discard_or_error(size_t) override;

    bool unreliable_eof() const override;
    bool handle_any_error() override;

    static Optional<ByteBuffer> decompress_all(ReadonlyBytes);

private:
    void decode_codes(CanonicalCode& literal_code, Optional<CanonicalCode>& command_code, Optional<CanonicalCode>& distance_code);

    bool m_read_final_bock { false };
    State m_state { State::ReadingStreamHeader };
    u32 m_max_backwards_reference { 0 };

    union {
        CompressedBlock m_compressed_block;
        UncompressedBlock m_uncompressed_block;
    };

    InputBitStream m_input_stream;
    Optional<HeapBackedCircularDuplexStream> m_sliding_window;
};

}
