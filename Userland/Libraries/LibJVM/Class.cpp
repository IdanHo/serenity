/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJVM/Class.h>

namespace JVM {

String Class::dump() const
{
    StringBuilder dump;

    dump.appendff("{}class {} {{\n", m_access, m_name);
    for (auto& field : m_fields)
        dump.appendff("  {}{}{};\n", field.access(), field.descriptor(), field.name());
    if (!m_fields.is_empty())
        dump.append('\n');
    for (auto& method : m_methods) {
        if (method.name() == "<init>")
            dump.appendff("  {}{};\n", m_name, method.argument_descriptors());
        else
            dump.appendff("  {}{} {}{};\n", method.access(), method.return_descriptor(), method.name(), method.argument_descriptors());
    }
    dump.append('}');

    return dump.build();
}

}
