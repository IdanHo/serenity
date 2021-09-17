/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/BinarySearch.h>
#include <LibCompress/Huffman.h>

namespace Compress {

static consteval u8 reverse8(u8 value)
{
    u8 result = 0;
    for (size_t i = 0; i < 8; i++) {
        if (value & (1 << i))
            result |= 1 << (7 - i);
    }
    return result;
}
static consteval Array<u8, UINT8_MAX + 1> generate_reverse8_lookup_table()
{
    Array<u8, UINT8_MAX + 1> array;
    for (size_t i = 0; i <= UINT8_MAX; i++) {
        array[i] = reverse8(i);
    }
    return array;
}
static constexpr auto reverse8_lookup_table = generate_reverse8_lookup_table();

// Lookup-table based bit swap
ALWAYS_INLINE static u16 fast_reverse16(u16 value, size_t bits)
{
    VERIFY(bits <= 16);

    u16 lo = value & 0xff;
    u16 hi = value >> 8;

    u16 reversed = (u16)((reverse8_lookup_table[lo] << 8) | reverse8_lookup_table[hi]);

    return reversed >> (16 - bits);
}

template<size_t MaxSymbols>
Optional<CanonicalCode<MaxSymbols>> CanonicalCode<MaxSymbols>::from_bytes(ReadonlyBytes bytes)
{
    // FIXME: I can't quite follow the algorithm here, but it seems to work.

    CanonicalCode code;

    auto non_zero_symbols = 0;
    auto last_non_zero = -1;
    for (size_t i = 0; i < bytes.size(); i++) {
        if (bytes[i] != 0) {
            non_zero_symbols++;
            last_non_zero = i;
        }
    }
    if (non_zero_symbols == 1) { // special case - only 1 symbol
        code.m_symbol_codes.append(0b10);
        code.m_symbol_values.append(last_non_zero);
        code.m_bit_codes[last_non_zero] = 0;
        code.m_bit_code_lengths[last_non_zero] = 1;
        return code;
    }

    auto next_code = 0;
    for (size_t code_length = 1; code_length <= 15; ++code_length) {
        next_code <<= 1;
        auto start_bit = 1 << code_length;

        for (size_t symbol = 0; symbol < bytes.size(); ++symbol) {
            if (bytes[symbol] != code_length)
                continue;

            if (next_code > start_bit)
                return {};

            code.m_symbol_codes.append(start_bit | next_code);
            code.m_symbol_values.append(symbol);
            code.m_bit_codes[symbol] = fast_reverse16(start_bit | next_code, code_length); // DEFLATE/Brotli write huffman encoded symbols as lsb-first
            code.m_bit_code_lengths[symbol] = code_length;

            next_code++;
        }
    }

    if (next_code != (1 << 15)) {
        return {};
    }

    return code;
}

template<size_t MaxSymbols>
u32 CanonicalCode<MaxSymbols>::read_symbol(InputBitStream& stream) const
{
    u32 code_bits = 1;

    for (;;) {
        code_bits = code_bits << 1 | stream.read_bits(1);
        if (code_bits >= (1 << 16))
            return AK::NumericLimits<u32>::max(); // We statically assert that these values is not a valid symbol, and is instead used to indicate an error

        // FIXME: This is very inefficient and could greatly be improved by implementing this
        //        algorithm: https://www.hanshq.net/zip.html#huffdec
        size_t index;
        if (binary_search(m_symbol_codes.span(), code_bits, &index))
            return m_symbol_values[index];
    }
}

template<size_t MaxSymbols>
void CanonicalCode<MaxSymbols>::write_symbol(OutputBitStream& stream, u32 symbol) const
{
    stream.write_bits(m_bit_codes[symbol], m_bit_code_lengths[symbol]);
}

}
