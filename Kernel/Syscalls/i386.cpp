/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Process.h>

namespace Kernel {

Atomic<bool> g_user_ldt_control { false };

// This syscall is a big foot-gun, but is required to support programs emulating old 16-bit code,
// in order to discourage the usage of the syscall and to preserve some sense of security when it's
// available the following security mitigations are applied:
//  * The syscall is completely unavailable until the "user_ldt_control" sysctl variable is set
//  * The syscall does not allow creating LDT entries with a DPL different from 3 (i.e. non-user descriptors)
//  * The syscall does not allow creating system segments and gate descriptors at all
//  * Provided descriptors are checked for validity based on the spec, to ensure the syscall will fail
//    with an error instead of the Process crashing with some CPU exception
ErrorOr<FlatPtr> Process::sys$i386_set_ldt(Userspace<Syscall::SC_i386_set_ldt_params const*> user_params)
{
    VERIFY_PROCESS_BIG_LOCK_ACQUIRED(this);
    TRY(require_promise(Pledge::i386));

    if (!g_user_ldt_control.load())
        return EACCES;

#if ARCH(I386) || ARCH(X86_64)
    auto params = TRY(copy_typed_from_user(user_params));

    if (params.index >= descriptor_table_entries)
        return EINVAL;

    // The type is only 4 bit wide
    if ((params.type & ~0b1111) != 0)
        return EINVAL;

    // Fourth bit is data when clear and code when set, all data segments are allowed
    if ((params.type & 0b1000) != 0) {
        // Third bit is the conforming flag for code segments, non-conforming code segments are not allowed
        if ((params.type & 0b0100) == 0)
            return EINVAL;

        // Code segments must be present
        if (params.present == 0)
            return EINVAL;
    }

    // The limit is only 20 bit wide
    if ((params.limit & ~0xFFFFF) != 0)
        return EINVAL;

    // The present flag is only 1 bit wide
    if ((params.present & ~0b1) != 0)
        return EINVAL;

    // The granularity flag is only 1 bit wide
    if ((params.granularity & ~0b1) != 0)
        return EINVAL;

    // The operation size flag is only 1 bit wide
    if ((params.operation_size_32bit & ~0b1) != 0)
        return EINVAL;

    // the u32 is intentional, as we do not support 64bit descriptors
    if (Checked<u32>::addition_would_overflow(params.base, params.limit))
        return EOVERFLOW;

    auto base_address = VirtualAddress(params.base);
    if (!Kernel::Memory::is_user_range(base_address, params.limit))
        return EFAULT;

    Descriptor ldt_entry {};
    ldt_entry.set_base(base_address);
    ldt_entry.set_limit(params.limit);
    ldt_entry.dpl = 3;
    ldt_entry.segment_present = params.present;
    ldt_entry.granularity = params.granularity;
    ldt_entry.operation_size64 = 0;
    ldt_entry.operation_size32 = params.operation_size_32bit;
    ldt_entry.descriptor_type = 1;
    ldt_entry.type = params.type;
    TRY(set_ldt_entry(params.index, ldt_entry));

    return 0;
#else
    return ENOTSUP;
#endif
}

}
