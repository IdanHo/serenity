/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <AK/Span.h>
#include <AK/StringBuilder.h>
#include <LibJVM/Class.h>
#include <LibJVM/ConstantPool.h>

namespace JVM {

constexpr u32 class_file_magic = 0xCAFEBABE;
constexpr u16 java_version_8_class = 0x52;

class ClassReader {
public:
    static Optional<Class> read(const ReadonlyBytes&);

private:
    explicit ClassReader(InputStream&);

    bool read_class_header(Class&);
    bool read_class_info(Class&);
    bool read_fields(Class&);
    bool read_methods(Class&);
    bool read_attributes(Class&);

    bool read_method_code(Method&);

    InputStream& m_input_stream;
    ConstantPool m_constant_pool;
};

}
