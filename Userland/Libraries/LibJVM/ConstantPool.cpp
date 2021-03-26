/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/String.h>
#include <LibJVM/ConstantPool.h>

namespace JVM {

ConstantPool::ConstantPool()
{
    auto zero_entry = make<ConstantPoolEntry>();
    zero_entry->tag = ConstantPoolTag::Invalid;
    m_constants.append(move(zero_entry)); // the constant pool is 1 indexed for some god awful reason
}

bool ConstantPool::load_constants(InputStream& input_stream)
{
    VERIFY(m_constants.size() == 1);
    BigEndian<u16> constant_pool_count;
    input_stream >> constant_pool_count;
    m_constants.ensure_capacity(constant_pool_count);
    for (size_t i = 1; i < constant_pool_count; i++) {
        BigEndian<u8> raw_constant_tag;
        input_stream >> raw_constant_tag;
        auto constant_pool_tag = static_cast<ConstantPoolTag>((u8)raw_constant_tag);
        switch (constant_pool_tag) {
        case ConstantPoolTag::UTF8: {
            auto entry = make<UTF8Entry>();
            entry->tag = constant_pool_tag;
            if (!convert_class_utf8(input_stream, entry->value))
                return false;
            m_constants.append(move(entry));
            break;
        }
        case ConstantPoolTag::Integer:
        case ConstantPoolTag::Float: {
            BigEndian<u32> bytes;
            input_stream >> bytes;

            auto entry = make<NumericEntry>();
            entry->tag = constant_pool_tag;
            entry->bytes = bytes;
            m_constants.append(move(entry));
            break;
        }
        case ConstantPoolTag::Long:
        case ConstantPoolTag::Double: {
            BigEndian<u32> high_bytes, low_bytes;
            input_stream >> high_bytes >> low_bytes;

            auto entry = make<LargeNumericEntry>();
            entry->tag = constant_pool_tag;
            entry->bytes = ((u64)high_bytes << 32) | low_bytes;
            m_constants.append(move(entry));

            auto filler_entry = make<ConstantPoolEntry>();
            filler_entry->tag = ConstantPoolTag::Invalid;
            m_constants.append(move(filler_entry));
            i += 1; // LargeNumericEntries "take up" 2 constant pool slots
            break;
        }
        case ConstantPoolTag::Class: {
            BigEndian<u16> name_index;
            input_stream >> name_index;
            if (name_index >= constant_pool_count)
                return false;

            auto entry = make<ClassEntry>();
            entry->tag = constant_pool_tag;
            entry->name_constant_index = name_index;
            m_constants.append(move(entry));
            break;
        }
        case ConstantPoolTag::String: {
            BigEndian<u16> string_index;
            input_stream >> string_index;
            if (string_index >= constant_pool_count)
                return false;

            auto entry = make<StringEntry>();
            entry->tag = constant_pool_tag;
            entry->utf8_constant_index = string_index;
            m_constants.append(move(entry));
            break;
        }
        case ConstantPoolTag::FieldReference:
        case ConstantPoolTag::MethodReference:
        case ConstantPoolTag::InterfaceMethodReference: {
            BigEndian<u16> class_index, name_and_type_index;
            input_stream >> class_index >> name_and_type_index;
            if (class_index >= constant_pool_count || name_and_type_index >= constant_pool_count)
                return false;

            auto entry = make<ReferenceEntry>();
            entry->tag = constant_pool_tag;
            entry->class_constant_index = class_index;
            entry->name_and_type_constant_index = name_and_type_index;
            m_constants.append(move(entry));
            break;
        }
        case ConstantPoolTag::NameAndType: {
            BigEndian<u16> name_index, descriptor_index;
            input_stream >> name_index >> descriptor_index;
            if (name_index >= constant_pool_count || descriptor_index >= constant_pool_count)
                return false;

            auto entry = make<NameAndTypeEntry>();
            entry->tag = constant_pool_tag;
            entry->name_constant_index = name_index;
            entry->descriptor_constant_index = descriptor_index;
            m_constants.append(move(entry));
            break;
        }
        case ConstantPoolTag::MethodHandle:
        case ConstantPoolTag::MethodType:
        case ConstantPoolTag::InvokeDynamic: {
            TODO(); // TODO: implement invoke dynamic
        }
        default:
            return false;
        }
    }
    VERIFY(m_constants.size() == constant_pool_count);
    return true;
}

bool ConstantPool::read_utf8_constant(size_t constant_index, String& result)
{
    if (constant_index >= m_constants.size()) {
        return false;
    }
    auto& constant_entry = m_constants[constant_index];
    if (constant_entry.tag != ConstantPoolTag::UTF8) {
        return false;
    }
    auto utf8_entry = static_cast<UTF8Entry&>(constant_entry);
    result = utf8_entry.value;
    return true;
}

bool ConstantPool::read_class_constant(size_t constant_index, String& name)
{
    if (constant_index >= m_constants.size())
        return false;
    auto& constant_entry = m_constants[constant_index];
    if (constant_entry.tag != ConstantPoolTag::Class)
        return false;
    auto class_entry = static_cast<ClassEntry&>(constant_entry);
    return read_utf8_constant(class_entry.name_constant_index, name);
}

bool ConstantPool::convert_class_utf8(InputStream& input_stream, String& result)
{
    BigEndian<u16> length;
    input_stream >> length;

    if (length == 0) {
        result = String::empty();
        return true;
    }

    char char_buffer[(u16)length];
    size_t string_length = 0;
    for (size_t i = 0; i < length; i++) {
        BigEndian<u8> current_byte;
        input_stream >> current_byte;
        if ((current_byte & 0x80) == 0) { // \u0001 - \u007F are represented by a single byte
            char_buffer[string_length++] = (char)(current_byte & 0x7F);
        } else if ((current_byte & 0xE0) == 0xC0) { // \u0000, \u0080 - \u07FF are represented by two bytes
            if (i + 1 >= length)
                return false;
            BigEndian<u8> second_byte;
            input_stream >> second_byte;
            if ((second_byte & 0xC0) != 0x80)
                return false;
            char_buffer[string_length++] = (char)((current_byte & 0x1F) << 6 | (second_byte & 0x3F));
            i += 1;
        } else if ((current_byte & 0xF0) == 0xE0) { // \u0800 - \uFFFF are represented by three bytes
            if (i + 2 >= length)
                return false;
            BigEndian<u8> second_byte, third_byte;
            input_stream >> second_byte >> third_byte;
            if ((second_byte & 0xC0) != 0x80 || (third_byte & 0xC0) != 0x80)
                return false;
            char_buffer[string_length++] = (char)((current_byte & 0x1F) << 12 | (second_byte & 0x3F) << 6 | (third_byte & 0x3F));
            i += 2;
        } else {
            TODO(); // UTF16 surrogates
        }
    }

    result = String { char_buffer, string_length };
    return true;
}

}
