/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJVM/Descriptor.h>

namespace JVM {

size_t Descriptor::from_raw(const String& raw_descriptor, Descriptor& descriptor)
{
    if (raw_descriptor.is_empty())
        return 0;
    auto parsed_type = static_cast<Type>(raw_descriptor[0]);
    switch (parsed_type) {
    case Type::Void:
    case Type::Byte:
    case Type::Char:
    case Type::Double:
    case Type::Float:
    case Type::Integer:
    case Type::Long:
    case Type::Short:
    case Type::Boolean: {
        descriptor = { parsed_type };
        return 1;
    }
    case Type::Reference: {
        if (raw_descriptor.length() < 3) // minimum reference descriptor: Lx;
            return 0;
        size_t end_offset;
        for (end_offset = 2; end_offset < raw_descriptor.length(); end_offset++) {
            if (raw_descriptor[end_offset] == ';')
                break;
        }
        if (end_offset == raw_descriptor.length())
            return 0;
        descriptor = { parsed_type, raw_descriptor.substring(1, end_offset - 1) };
        return end_offset;
    }
    case Type::Array: {
        u8 dimensions = 0;
        for (size_t i = 0; i < raw_descriptor.length(); i++) {
            if (raw_descriptor[i] != '[')
                break;
            dimensions++;
        }
        auto elements_descriptor = make<Descriptor>();
        auto descriptor_length = from_raw(raw_descriptor.substring(dimensions), *elements_descriptor);
        if (!descriptor_length)
            return 0;
        descriptor = Descriptor { parsed_type, dimensions, move(elements_descriptor) };
        return dimensions + descriptor_length;
    }
    default:
        return false;
    }
}

bool Descriptor::from_raw_method(const String& raw_descriptor, Descriptor& return_descriptor, Vector<Descriptor>& argument_descriptors)
{
    if (raw_descriptor.length() < 3 || raw_descriptor[0] != '(') // minimum method descriptor: ()V
        return false;
    size_t offset;
    for (offset = 1; offset < raw_descriptor.length(); offset++) {
        if (raw_descriptor[offset] == ')')
            break;
        Descriptor argument;
        auto descriptor_length = from_raw(raw_descriptor.substring(offset), argument);
        if (!descriptor_length)
            return false;
        offset += descriptor_length;
        argument_descriptors.append(move(argument));
    }
    auto raw_return_descriptor = raw_descriptor.substring(offset + 1);
    return from_raw(raw_return_descriptor, return_descriptor) == raw_return_descriptor.length();
}

}
