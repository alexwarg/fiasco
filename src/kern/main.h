#pragma once

#include "std_macros.h"

extern "C" void kernel_main();
int FIASCO_FASTCALL boot_ap_cpu() __asm__("BOOT_AP_CPU");

