/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/String.h>
#include <LibJVM/Access.h>
#include <LibJVM/Field.h>
#include <LibJVM/Method.h>

namespace JVM {

class Class {
public:
    String dump() const;

private:
    Class() = default;

    friend class ClassReader;

    u16 m_format_version { 0 };
    ClassAccess m_access { 0 };

    String m_name { "" };
    String m_super_class { "" };
    Vector<String> m_interfaces;

    Vector<Field> m_fields;
    Vector<Method> m_methods;
};

}
