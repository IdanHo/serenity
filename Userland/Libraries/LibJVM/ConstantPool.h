/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Endian.h>
#include <AK/NonnullOwnPtrVector.h>

namespace JVM {

class ConstantPool {
public:
    ConstantPool();

    bool load_constants(InputStream&);

    bool read_utf8_constant(size_t constant_index, String& result);
    bool read_class_constant(size_t constant_index, String& name);

private:
    static bool convert_class_utf8(InputStream&, String& result);

    enum class ConstantPoolTag : u8 {
        Invalid = 0,
        UTF8 = 1,
        Integer = 3,
        Float = 4,
        Long = 5,
        Double = 6,
        Class = 7,
        String = 8,
        FieldReference = 9,
        MethodReference = 10,
        InterfaceMethodReference = 11,
        NameAndType = 12,
        MethodHandle = 15,
        MethodType = 16,
        InvokeDynamic = 18
    };

    struct ConstantPoolEntry {
        ConstantPoolTag tag;
    };

    struct ClassEntry : ConstantPoolEntry {
        u16 name_constant_index;
    };

    struct ReferenceEntry : ConstantPoolEntry {
        u16 class_constant_index;
        u16 name_and_type_constant_index;
    };

    struct StringEntry : ConstantPoolEntry {
        u16 utf8_constant_index;
    };

    struct NumericEntry : ConstantPoolEntry {
        u32 bytes;
    };

    struct LargeNumericEntry : ConstantPoolEntry {
        u64 bytes;
    };

    struct NameAndTypeEntry : ConstantPoolEntry {
        u16 name_constant_index;
        u16 descriptor_constant_index;
    };

    struct UTF8Entry : ConstantPoolEntry {
        String value;
    };

    NonnullOwnPtrVector<ConstantPoolEntry> m_constants;
};

}
