/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/EnumBits.h>
#include <AK/String.h>

namespace JVM {

enum class ClassAccess : u16 {
    PUBLIC = 0x1,
    FINAL = 0x10,
    SUPER = 0x20,
    INTERFACE = 0x200,
    ABSTRACT = 0x400,
    SYNTHETIC = 0x1000,
    ANNOTATION = 0x2000,
    ENUM = 0x4000
};

AK_ENUM_BITWISE_OPERATORS(ClassAccess);

enum class FieldAccess : u16 {
    PUBLIC = 0x1,
    PRIVATE = 0x2,
    PROTECTED = 0x4,
    STATIC = 0x8,
    FINAL = 0x10,
    VOLATILE = 0x40,
    TRANSIENT = 0x80,
    SYNTHETIC = 0x1000,
    ENUM = 0x4000
};

AK_ENUM_BITWISE_OPERATORS(FieldAccess);

enum class MethodAccess : u16 {
    PUBLIC = 0x1,
    PRIVATE = 0x2,
    PROTECTED = 0x4,
    STATIC = 0x8,
    FINAL = 0x10,
    SYNCHRONIZED = 0x20,
    BRIDGE = 0x40,
    VARARGS = 0x80,
    NATIVE = 0x100,
    ABSTRACT = 0x400,
    STRICT = 0x800,
    SYNTHETIC = 0x1000
};

AK_ENUM_BITWISE_OPERATORS(MethodAccess);

}

namespace AK {

template<>
struct Formatter<JVM::ClassAccess> : StandardFormatter {
    static void format(FormatBuilder& builder, JVM::ClassAccess value)
    {
        if (has_flag(value, JVM::ClassAccess::PUBLIC))
            builder.put_literal("public ");
        if (has_flag(value, JVM::ClassAccess::ABSTRACT))
            builder.put_literal("abstract ");
        if (has_flag(value, JVM::ClassAccess::FINAL))
            builder.put_literal("final ");
    }
};

template<>
struct Formatter<JVM::FieldAccess> : StandardFormatter {
    static void format(FormatBuilder& builder, JVM::FieldAccess value)
    {
        if (has_flag(value, JVM::FieldAccess::PUBLIC))
            builder.put_literal("public ");
        if (has_flag(value, JVM::FieldAccess::PROTECTED))
            builder.put_literal("protected ");
        if (has_flag(value, JVM::FieldAccess::PRIVATE))
            builder.put_literal("private ");
        if (has_flag(value, JVM::FieldAccess::STATIC))
            builder.put_literal("static ");
        if (has_flag(value, JVM::FieldAccess::FINAL))
            builder.put_literal("final ");
        if (has_flag(value, JVM::FieldAccess::TRANSIENT))
            builder.put_literal("transient ");
        if (has_flag(value, JVM::FieldAccess::VOLATILE))
            builder.put_literal("volatile ");
    }
};

template<>
struct Formatter<JVM::MethodAccess> : StandardFormatter {
    static void format(FormatBuilder& builder, JVM::MethodAccess value)
    {
        if (has_flag(value, JVM::MethodAccess::PUBLIC))
            builder.put_literal("public ");
        if (has_flag(value, JVM::MethodAccess::PROTECTED))
            builder.put_literal("protected ");
        if (has_flag(value, JVM::MethodAccess::PRIVATE))
            builder.put_literal("private ");
        if (has_flag(value, JVM::MethodAccess::ABSTRACT))
            builder.put_literal("abstract ");
        if (has_flag(value, JVM::MethodAccess::STATIC))
            builder.put_literal("static ");
        if (has_flag(value, JVM::MethodAccess::FINAL))
            builder.put_literal("final ");
        if (has_flag(value, JVM::MethodAccess::SYNCHRONIZED))
            builder.put_literal("synchronized ");
        if (has_flag(value, JVM::MethodAccess::NATIVE))
            builder.put_literal("native ");
        if (has_flag(value, JVM::MethodAccess::STRICT))
            builder.put_literal("strictfp ");
    }
};

}
