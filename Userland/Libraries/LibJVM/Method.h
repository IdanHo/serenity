/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/String.h>
#include <AK/Vector.h>
#include <LibJVM/Access.h>
#include <LibJVM/Descriptor.h>

namespace JVM {

class Method {
public:
    const String& name() const { return m_name; }
    const Descriptor& return_descriptor() const { return m_return_descriptor; }
    const Vector<Descriptor>& argument_descriptors() const { return m_argument_descriptors; }
    MethodAccess access() const { return m_access; }

private:
    Method() = default;

    friend class ClassReader;

    String m_name { "" };
    Descriptor m_return_descriptor;
    Vector<Descriptor> m_argument_descriptors;
    MethodAccess m_access { 0 };

    u16 m_maximum_stack_size;
    u16 m_maximum_local_count;
    ByteBuffer m_code; // FIXME: actually do something with this

    struct ExceptionHandler {
        u16 range_start_offset;
        u16 range_end_offset;
        u16 handler_offset;
        String exception_type;
    };
    Vector<ExceptionHandler> m_exception_handlers;
};

}
