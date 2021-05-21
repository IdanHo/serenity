/*
 * Copyright (c) 2020, Liav A. <liavalb@hotmail.co.il>
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/ACPI/Parser.h>
#include <Kernel/CommandLine.h>
#include <Kernel/Sections.h>

namespace Kernel {
namespace ACPI {

UNMAP_AFTER_INIT void initialize()
{
    if (kernel_command_line().acpi_feature_level() == AcpiFeatureLevel::Disabled)
        return;

    auto rsdp = StaticParsing::find_rsdp();
    if (!rsdp.has_value())
        return;

    Parser::initialize(rsdp.value());
}

bool is_enabled()
{
    return Parser::the();
}

}
}
