/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/String.h>
#include <LibJVM/Access.h>
#include <LibJVM/Descriptor.h>

namespace JVM {

class Field {
public:
    const String& name() const { return m_name; }
    const Descriptor& descriptor() const { return m_descriptor; }
    FieldAccess access() const { return m_access; }

private:
    Field() = default;

    friend class ClassReader;
    String m_name { "" };
    Descriptor m_descriptor;
    FieldAccess m_access { 0 };
};

}
