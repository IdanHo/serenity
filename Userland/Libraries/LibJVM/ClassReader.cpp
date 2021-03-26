/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Endian.h>
#include <AK/MemoryStream.h>
#include <LibJVM/ClassReader.h>

namespace JVM {

Optional<Class> ClassReader::read(const ReadonlyBytes& class_bytes)
{
    InputMemoryStream class_stream { class_bytes };
    ClassReader reader { class_stream };
    Class read_class;
    if (!reader.read_class_header(read_class))
        return {};
    if (!reader.m_constant_pool.load_constants(class_stream))
        return {};
    if (!reader.read_class_info(read_class))
        return {};
    if (!reader.read_fields(read_class))
        return {};
    if (!reader.read_methods(read_class))
        return {};
    if (!reader.read_attributes(read_class))
        return {};
    if (class_stream.handle_any_error())
        return {};

    return read_class;
}

ClassReader::ClassReader(InputStream& stream)
    : m_input_stream(stream)
{
}

bool ClassReader::read_class_header(Class& read_class)
{
    BigEndian<u32> magic;
    m_input_stream >> magic;
    if (magic != class_file_magic)
        return false;
    BigEndian<u16> minor_version, major_version;
    m_input_stream >> minor_version >> major_version;
    if (major_version > java_version_8_class)
        return false; // TODO: support newer classes
    read_class.m_format_version = major_version;
    return true;
}

bool ClassReader::read_class_info(Class& read_class)
{
    BigEndian<u16> access_flags;
    m_input_stream >> access_flags;
    auto class_access_flags = static_cast<ClassAccess>((u16)access_flags);
    if (has_flag(class_access_flags, ClassAccess::INTERFACE)) {
        if (!has_flag(class_access_flags, ClassAccess::ABSTRACT))
            return false; // an interface is always abstract
        if (has_flag(class_access_flags, ClassAccess::FINAL))
            return false; // an interface can not be final
        if (has_flag(class_access_flags, ClassAccess::SUPER))
            return false; // an interface doesnt have a super class
        if (has_flag(class_access_flags, ClassAccess::ENUM))
            return false; // an interface cannot be an enum
    } else {
        class_access_flags |= ClassAccess::SUPER; // the specification recommends treating this flag as enabled for all classes
    }
    if (has_flag(class_access_flags, ClassAccess::ABSTRACT) && has_flag(class_access_flags, ClassAccess::FINAL))
        return false; // a final class cant have unimplemented methods
    if (has_flag(class_access_flags, ClassAccess::ANNOTATION) && !has_flag(class_access_flags, ClassAccess::INTERFACE))
        return false; // an annotation class must be an interface
    read_class.m_access = class_access_flags;

    BigEndian<u16> this_class_index;
    m_input_stream >> this_class_index;
    if (!m_constant_pool.read_class_constant(this_class_index, read_class.m_name))
        return false;

    BigEndian<u16> super_class_index;
    m_input_stream >> super_class_index;
    if (!m_constant_pool.read_class_constant(super_class_index, read_class.m_super_class))
        return false;

    BigEndian<u16> interfaces_count;
    m_input_stream >> interfaces_count;
    for (size_t i = 0; i < interfaces_count; i++) {
        BigEndian<u16> interface_index;
        m_input_stream >> interface_index;
        String interface;
        if (!m_constant_pool.read_class_constant(interface_index, interface))
            return false;
        read_class.m_interfaces.append(interface);
    }
    return true;
}

bool ClassReader::read_fields(Class& read_class)
{
    BigEndian<u16> fields_count;
    m_input_stream >> fields_count;
    for (size_t i = 0; i < fields_count; i++) {
        Field read_field;

        BigEndian<u16> access_flags;
        m_input_stream >> access_flags;
        auto field_access_flags = static_cast<FieldAccess>((u16)access_flags);
        if (has_flag(field_access_flags, FieldAccess::PUBLIC) && has_flag(field_access_flags, FieldAccess::PROTECTED | FieldAccess::PRIVATE))
            return false; // a public field cannot be private or protected
        if (has_flag(field_access_flags, FieldAccess::PROTECTED) && has_flag(field_access_flags, FieldAccess::PRIVATE))
            return false; // a protected field cannot be private
        if (has_flag(field_access_flags, FieldAccess::FINAL) && has_flag(field_access_flags, FieldAccess::VOLATILE))
            return false; // a final field cannot be volatile
        if (has_flag(read_class.m_access, ClassAccess::INTERFACE) && (!has_flag(field_access_flags, FieldAccess::PUBLIC) || !has_flag(field_access_flags, FieldAccess::STATIC) || !has_flag(field_access_flags, FieldAccess::FINAL)))
            return false; // an interface field must be public, static and final
        read_field.m_access = field_access_flags;

        BigEndian<u16> name_index;
        m_input_stream >> name_index;
        if (!m_constant_pool.read_utf8_constant(name_index, read_field.m_name))
            return false;

        String field_descriptor;
        BigEndian<u16> descriptor_index;
        m_input_stream >> descriptor_index;
        if (!m_constant_pool.read_utf8_constant(descriptor_index, field_descriptor))
            return false;
        if (!Descriptor::from_raw(field_descriptor, read_field.m_descriptor))
            return false;

        BigEndian<u16> attributes_count;
        m_input_stream >> attributes_count;
        for (size_t attribute_index = 0; attribute_index < attributes_count; attribute_index++) {
            String attribute_name;

            BigEndian<u16> attribute_name_index;
            m_input_stream >> attribute_name_index;
            if (!m_constant_pool.read_utf8_constant(attribute_name_index, attribute_name))
                return false;

            BigEndian<u32> length;
            m_input_stream >> length;

            if (attribute_name == "ConstantValue") {
                TODO();
            } else if (attribute_name == "Synthetic") {
                TODO();
            } else if (attribute_name == "Deprecated") {
                TODO();
            } else if (attribute_name == "Signature") {
                TODO();
            } else if (attribute_name == "RuntimeVisibleAnnotations") {
                TODO();
            } else if (attribute_name == "RuntimeInvisibleAnnotations") {
                TODO();
            } else if (attribute_name == "RuntimeVisibleTypeAnnotations") {
                TODO();
            } else if (attribute_name == "RuntimeInvisibleTypeAnnotations") {
                TODO();
            } else {
                dbgln("Unknown Field Attribute: {}", attribute_name);
                m_input_stream.discard_or_error(length);
            }
        }

        read_class.m_fields.append(move(read_field));
    }
    return true;
}

bool ClassReader::read_methods(Class& read_class)
{
    BigEndian<u16> methods_count;
    m_input_stream >> methods_count;
    for (size_t i = 0; i < methods_count; i++) {
        Method read_method;

        BigEndian<u16> access_flags;
        m_input_stream >> access_flags;
        auto method_access_flags = static_cast<MethodAccess>((u16)access_flags);
        if (has_flag(method_access_flags, MethodAccess::PUBLIC) && has_flag(method_access_flags, MethodAccess::PROTECTED | MethodAccess::PRIVATE))
            return false; // a public method cannot be private or protected
        if (has_flag(method_access_flags, MethodAccess::PROTECTED) && has_flag(method_access_flags, MethodAccess::PRIVATE))
            return false; // a protected method cannot be private
        if (has_flag(read_class.m_access, ClassAccess::INTERFACE)) {
            if (has_flag(method_access_flags, MethodAccess::PROTECTED | MethodAccess::FINAL | MethodAccess::SYNCHRONIZED | MethodAccess::NATIVE))
                return false; // an interface method may not be protected, final, synchronized or native
            if (read_class.m_format_version < java_version_8_class) {
                if (!has_flag(method_access_flags, MethodAccess::PUBLIC) || !has_flag(method_access_flags, MethodAccess::ABSTRACT))
                    return false; // an old interface method must be both public and abstract
            } else {
                // a modern interface method must be either public or private
                if (has_flag(method_access_flags, MethodAccess::PUBLIC)) {
                    if (has_flag(method_access_flags, MethodAccess::PRIVATE))
                        return false;
                } else if (!has_flag(method_access_flags, MethodAccess::PRIVATE)) {
                    return false;
                }
            }
            // an abstract interface method may not be private, static, final, synchronized, native or strict
            if (has_flag(method_access_flags, MethodAccess::ABSTRACT) && has_flag(method_access_flags, MethodAccess::PRIVATE | MethodAccess::STATIC | MethodAccess::FINAL | MethodAccess::SYNCHRONIZED | MethodAccess::NATIVE | MethodAccess::STRICT))
                return false;
        }
        read_method.m_access = method_access_flags;

        BigEndian<u16> name_index;
        m_input_stream >> name_index;
        if (!m_constant_pool.read_utf8_constant(name_index, read_method.m_name))
            return false;

        if (read_method.m_name == "<init>" && has_flag(read_method.m_access, MethodAccess::STRICT | MethodAccess::FINAL | MethodAccess::SYNCHRONIZED | MethodAccess::BRIDGE | MethodAccess::NATIVE | MethodAccess::ABSTRACT))
            return false; // an instance initialization method may not be strict, final, synchronized, bridge, native or abstract

        String method_descriptor;
        BigEndian<u16> descriptor_index;
        m_input_stream >> descriptor_index;
        if (!m_constant_pool.read_utf8_constant(descriptor_index, method_descriptor))
            return false;
        if (!Descriptor::from_raw_method(method_descriptor, read_method.m_return_descriptor, read_method.m_argument_descriptors))
            return false;
        BigEndian<u16> attributes_count;
        m_input_stream >> attributes_count;

        bool code_attribute_seen = has_flag(read_method.m_access, MethodAccess::NATIVE | MethodAccess::ABSTRACT);
        for (size_t attribute_index = 0; attribute_index < attributes_count; attribute_index++) {
            String attribute_name;

            BigEndian<u16> attribute_name_index;
            m_input_stream >> attribute_name_index;
            if (!m_constant_pool.read_utf8_constant(attribute_name_index, attribute_name))
                return false;

            BigEndian<u32> length;
            m_input_stream >> length;

            if (attribute_name == "Code") {
                if (has_flag(read_method.m_access, MethodAccess::NATIVE | MethodAccess::ABSTRACT))
                    return false;
                code_attribute_seen = true;
                if (!read_method_code(read_method))
                    return false;
            } else if (attribute_name == "Exceptions") {
                TODO();
            } else if (attribute_name == "RuntimeVisibleParameterAnnotations") {
                TODO();
            } else if (attribute_name == "RuntimeInvisibleParameterAnnotations") {
                TODO();
            } else if (attribute_name == "AnnotationDefault") {
                TODO();
            } else if (attribute_name == "MethodParameters") {
                TODO();
            } else if (attribute_name == "Synthetic") {
                TODO();
            } else if (attribute_name == "Deprecated") {
                TODO();
            } else if (attribute_name == "Signature") {
                TODO();
            } else if (attribute_name == "RuntimeVisibleAnnotations") {
                TODO();
            } else if (attribute_name == "RuntimeInvisibleAnnotations") {
                TODO();
            } else if (attribute_name == "RuntimeVisibleTypeAnnotations") {
                TODO();
            } else if (attribute_name == "RuntimeInvisibleTypeAnnotations") {
                TODO();
            } else {
                dbgln("Unknown Method Attribute: {}", attribute_name);
                m_input_stream.discard_or_error(length);
            }
        }

        if (!code_attribute_seen)
            return false;

        read_class.m_methods.append(move(read_method));
    }

    return true;
}

bool ClassReader::read_attributes(Class&)
{
    BigEndian<u16> attributes_count;
    m_input_stream >> attributes_count;
    for (size_t i = 0; i < attributes_count; i++) {
        String attribute_name;

        BigEndian<u16> name_index;
        m_input_stream >> name_index;
        if (!m_constant_pool.read_utf8_constant(name_index, attribute_name))
            return false;

        BigEndian<u32> length;
        m_input_stream >> length;

        if (attribute_name == "SourceFile") {
            TODO();
        } else if (attribute_name == "InnerClasses") {
            TODO();
        } else if (attribute_name == "EnclosingMethod") {
            TODO();
        } else if (attribute_name == "SourceDebugExtension") {
            TODO();
        } else if (attribute_name == "BootstrapMethods") {
            TODO();
        } else if (attribute_name == "Synthetic") {
            TODO();
        } else if (attribute_name == "Deprecated") {
            TODO();
        } else if (attribute_name == "Signature") {
            TODO();
        } else if (attribute_name == "RuntimeVisibleAnnotations") {
            TODO();
        } else if (attribute_name == "RuntimeInvisibleAnnotations") {
            TODO();
        } else if (attribute_name == "RuntimeVisibleTypeAnnotations") {
            TODO();
        } else if (attribute_name == "RuntimeInvisibleTypeAnnotations") {
            TODO();
        } else {
            dbgln("Unknown Class Attribute: {}", attribute_name);
            m_input_stream.discard_or_error(length);
        }
    }
    return true;
}
bool ClassReader::read_method_code(Method& read_method)
{
    BigEndian<u16> max_stack, max_locals;
    m_input_stream >> max_stack >> max_locals;
    read_method.m_maximum_stack_size = max_stack;
    read_method.m_maximum_local_count = max_locals;

    BigEndian<u32> code_length;
    m_input_stream >> code_length;
    if (code_length >= UINT16_MAX)
        return false; // why the field is 32 bit but verified to fit in 16 bit is beyond me

    read_method.m_code = ByteBuffer::create_uninitialized(code_length);
    m_input_stream >> read_method.m_code; // FIXME: actually do something with this

    BigEndian<u16> exception_table_length;
    m_input_stream >> exception_table_length;
    for (size_t i = 0; i < exception_table_length; i++) {
        Method::ExceptionHandler read_exception_handler;

        BigEndian<u16> start_pc, end_pc, handler_pc;
        m_input_stream >> start_pc >> end_pc >> handler_pc;
        if (start_pc >= code_length || end_pc >= code_length || handler_pc >= code_length)
            return false;
        read_exception_handler.range_start_offset = start_pc;
        read_exception_handler.range_end_offset = end_pc;
        read_exception_handler.handler_offset = handler_pc;

        BigEndian<u16> catch_type;
        m_input_stream >> catch_type;
        if (catch_type != 0 && !m_constant_pool.read_utf8_constant(catch_type, read_exception_handler.exception_type))
            return false;
        // we dont initialize exception_type if this is a catch-all handler

        read_method.m_exception_handlers.append(move(read_exception_handler));
    }

    BigEndian<u16> attributes_count;
    m_input_stream >> attributes_count;
    for (size_t i = 0; i < attributes_count; i++) {
        String attribute_name;

        BigEndian<u16> name_index;
        m_input_stream >> name_index;
        if (!m_constant_pool.read_utf8_constant(name_index, attribute_name))
            return false;

        BigEndian<u32> length;
        m_input_stream >> length;

        if (attribute_name == "LineNumberTable") {
            TODO();
        } else if (attribute_name == "LocalVariableTable") {
            TODO();
        } else if (attribute_name == "LocalVariableTypeTable") {
            TODO();
        } else if (attribute_name == "StackMapTable") {
            TODO();
        } else {
            dbgln("Unknown Code Attribute: {}", attribute_name);
            m_input_stream.discard_or_error(length);
        }
    }

    return true;
}

}
