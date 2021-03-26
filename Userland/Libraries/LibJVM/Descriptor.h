/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/OwnPtr.h>
#include <AK/String.h>
#include <AK/Vector.h>

namespace JVM {

// FIXME: This is a bit of a mess, and should probably be rewritten

enum class Type : char {
    Void = 'V',
    Byte = 'B',
    Char = 'C',
    Double = 'D',
    Float = 'F',
    Integer = 'I',
    Long = 'J',
    Short = 'S',
    Boolean = 'Z',
    Reference = 'L',
    Array = '['
};

class Descriptor {
public:
    Descriptor() = default;

    static size_t from_raw(const String& raw_descriptor, Descriptor& descriptor);
    static bool from_raw_method(const String& raw_descriptor, Descriptor& return_descriptor, Vector<Descriptor>& argument_descriptors);

private:
    Descriptor(Type type)
        : m_type(type) {};
    Descriptor(Type type, String class_name)
        : m_type(type)
        , m_class_name(class_name) {};
    Descriptor(Type type, u8 dimensions, OwnPtr<Descriptor> elements_descriptor)
        : m_type(type)
        , m_dimensions(dimensions)
        , m_elements_descriptor(move(elements_descriptor))
    {
        VERIFY(m_elements_descriptor);
    };
    Type m_type { Type::Void };
    String m_class_name {};
    u8 m_dimensions { 0 };
    OwnPtr<Descriptor> m_elements_descriptor { nullptr };

    friend struct AK::Formatter<JVM::Descriptor>;
};

}

namespace AK {

template<>
struct Formatter<JVM::Descriptor> : StandardFormatter {
    static void format(FormatBuilder& builder, const JVM::Descriptor& value)
    {
        switch (value.m_type) {
        case JVM::Type::Void:
            return builder.put_literal("void");
        case JVM::Type::Byte:
            return builder.put_literal("byte");
        case JVM::Type::Char:
            return builder.put_literal("char");
        case JVM::Type::Double:
            return builder.put_literal("double");
        case JVM::Type::Float:
            return builder.put_literal("float");
        case JVM::Type::Integer:
            return builder.put_literal("int");
        case JVM::Type::Long:
            return builder.put_literal("long");
        case JVM::Type::Short:
            return builder.put_literal("short");
        case JVM::Type::Boolean:
            return builder.put_literal("boolean");
        case JVM::Type::Reference: {
            auto pretty_name = value.m_class_name;
            pretty_name.replace("/", ".", true);
            return builder.put_literal(pretty_name);
        }
        case JVM::Type::Array: {
            Formatter<JVM::Descriptor>::format(builder, *value.m_elements_descriptor);
            for (size_t i = 0; i < value.m_dimensions; i++) {
                builder.put_literal("[]");
            }
            return;
        }
        }
        VERIFY_NOT_REACHED();
    }
};

template<>
struct Formatter<Vector<JVM::Descriptor>> : StandardFormatter {
    static void format(FormatBuilder& builder, const Vector<JVM::Descriptor>& value)
    {
        builder.put_literal("(");
        for (size_t i = 0; i < value.size(); i++) {
            Formatter<JVM::Descriptor>::format(builder, value[i]);
            if (i != value.size() - 1)
                builder.put_literal(", ");
        }
        builder.put_literal(")");
    }
};

}
