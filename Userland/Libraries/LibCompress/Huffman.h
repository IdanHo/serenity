/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Array.h>
#include <AK/BitStream.h>
#include <AK/NumericLimits.h>
#include <AK/Optional.h>
#include <AK/Vector.h>

#pragma once

namespace Compress {

template<size_t MaxSymbols>
class CanonicalCode {
    static_assert(MaxSymbols < AK::NumericLimits<u32>::max());

public:
    CanonicalCode() = default;
    u32 read_symbol(InputBitStream&) const;
    void write_symbol(OutputBitStream&, u32) const;

    static Optional<CanonicalCode> from_bytes(ReadonlyBytes);

private:
    // Decompression - indexed by code
    Vector<u16> m_symbol_codes;
    Vector<u16> m_symbol_values;

    // Compression - indexed by symbol
    Array<u16, MaxSymbols> m_bit_codes {};
    Array<u16, MaxSymbols> m_bit_code_lengths {};
};

// DEFLATE codes
template class CanonicalCode<19>;
template class CanonicalCode<32>;
template class CanonicalCode<288>;

}
