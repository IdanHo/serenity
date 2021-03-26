/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/MappedFile.h>
#include <LibCore/ArgsParser.h>
#include <LibJVM/ClassReader.h>

int main(int argc, char** argv)
{
    const char* path;

    Core::ArgsParser args_parser;
    args_parser.add_positional_argument(path, "Java class file to dump", "path", Core::ArgsParser::Required::Yes);
    args_parser.parse(argc, argv);

    String class_file_path { path };

    auto file_or_error = MappedFile::map(class_file_path);
    if (file_or_error.is_error()) {
        warnln("Failed to open {}: {}", class_file_path, file_or_error.error());
        return 1;
    }
    auto& mapped_file = *file_or_error.value();

    auto parsed_class = JVM::ClassReader::read(mapped_file.bytes());
    if (!parsed_class.has_value()) {
        warnln("Failed parsing class file!");
        return 1;
    }
    outln("{}", parsed_class->dump());

    return 0;
}
