# Patches for wine on SerenityOS

## `0001-ntdll-Don-t-use-fstatfs.patch`

ntdll: Don't use fstatfs


## `0002-ntdll-libwine-Add-serenity-s-i386_set_ldt-to-LDT-fun.patch`

ntdll+libwine: Add serenity's i386_set_ldt to LDT functions


## `0003-Everywhere-Use-proc-self-exe-on-serenity.patch`

Everywhere: Use /proc/self/exe on serenity

Add serenity to the list of operating systems that support the
/proc/self/exe file.

## `0004-ndtll-Use-serenity-s-set_process_name-libc-helper.patch`

ndtll: Use serenity's set_process_name libc helper


## `0005-ntdll-Stub-out-ucontext-on-serenity.patch`

ntdll: Stub out ucontext on serenity


## `0006-server-ntdll-Use-serenity-s-fd-sending-API.patch`

server+ntdll: Use serenity's fd sending API


