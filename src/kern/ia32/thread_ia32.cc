
#include <thread.h>
#include <cstdio>
#include <feature.h>

KIP_KERNEL_FEATURE("segments");

Trap_state::Handler Thread::nested_trap_handler FIASCO_FASTCALL;

