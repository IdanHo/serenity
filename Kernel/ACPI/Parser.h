/*
 * Copyright (c) 2020, Liav A. <liavalb@hotmail.co.il>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>
#include <Kernel/ACPI/Definitions.h>
#include <Kernel/ACPI/Initialize.h>
#include <Kernel/FileSystem/File.h>
#include <Kernel/PhysicalAddress.h>
#include <Kernel/VM/Region.h>
#include <Kernel/VirtualAddress.h>

namespace Kernel {
namespace ACPI {

class Parser final {
public:
    static Parser* the();

    static void initialize(PhysicalAddress rsdp)
    {
        set_the(*new Parser(rsdp));
    }

    PhysicalAddress find_table(const StringView& signature);

    void try_acpi_reboot();
    bool can_reboot();
    void try_acpi_shutdown();
    bool can_shutdown() { return false; }

    virtual bool have_8042() const
    {
        return m_x86_specific_flags.keyboard_8042;
    }

    const FADTFlags::HardwareFeatures& hardware_features() const { return m_hardware_flags; }
    const FADTFlags::x86_Specific_Flags& x86_specific_flags() const { return m_x86_specific_flags; }

private:
    explicit Parser(PhysicalAddress rsdp);
    virtual ~Parser() = default;

    static void set_the(Parser&);

    void locate_static_data();
    void locate_main_system_description_table();
    void initialize_main_system_description_table();
    size_t get_table_size(PhysicalAddress);
    u8 get_table_revision(PhysicalAddress);
    void init_fadt();

    bool validate_reset_register();
    void access_generic_address(const Structures::GenericAddressStructure&, u32 value);

    PhysicalAddress m_rsdp;
    PhysicalAddress m_main_system_description_table;

    Vector<PhysicalAddress> m_sdt_pointers;
    PhysicalAddress m_fadt;
    PhysicalAddress m_facs;

    bool m_xsdt_supported { false };
    FADTFlags::HardwareFeatures m_hardware_flags;
    FADTFlags::x86_Specific_Flags m_x86_specific_flags;
};

}
}
