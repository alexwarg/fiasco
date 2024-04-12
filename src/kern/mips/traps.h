
#pragma once

#include <types.h>

class Trap_state;

int call_nested_trap_handler(Trap_state *ts);

extern "C" FIASCO_FASTCALL
void thread_handle_trap(Mword cause, Trap_state *ts);

extern "C"
void handle_fpu_trap(Trap_state::Cause cause, Trap_state *ts);

extern "C" FIASCO_FASTCALL
void thread_handle_tlb_fault(Mword cause, Trap_state *ts, Mword pfa);

extern "C" FIASCO_FASTCALL
void thread_unhandled_trap(Mword, Trap_state *ts);



