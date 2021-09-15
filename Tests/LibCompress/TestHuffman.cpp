/*
 * Copyright (c) 2020-2021, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibTest/TestCase.h>

#include <AK/Array.h>
#include <AK/MemoryStream.h>
#include <AK/Random.h>
#include <LibCompress/Huffman.h>

TEST_CASE(canonical_code_simple)
{
    const Array<u8, 32> code {
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05
    };
    const Array<u8, 6> input {
        0x00, 0x42, 0x84, 0xa9, 0xb0, 0x15
    };
    const Array<u32, 9> output {
        0x00, 0x01, 0x01, 0x02, 0x03, 0x05, 0x08, 0x0d, 0x15
    };

    const auto huffman = Compress::CanonicalCode::from_bytes(code).value();
    auto memory_stream = InputMemoryStream { input };
    auto bit_stream = InputBitStream { memory_stream };

    for (size_t idx = 0; idx < 9; ++idx)
        EXPECT_EQ(huffman.read_symbol(bit_stream), output[idx]);
}

TEST_CASE(canonical_code_complex)
{
    const Array<u8, 6> code {
        0x03, 0x02, 0x03, 0x03, 0x02, 0x03
    };
    const Array<u8, 4> input {
        0xa1, 0xf3, 0xa1, 0xf3
    };
    const Array<u32, 12> output {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05
    };

    const auto huffman = Compress::CanonicalCode::from_bytes(code).value();
    auto memory_stream = InputMemoryStream { input };
    auto bit_stream = InputBitStream { memory_stream };

    for (size_t idx = 0; idx < 12; ++idx)
        EXPECT_EQ(huffman.read_symbol(bit_stream), output[idx]);
}
