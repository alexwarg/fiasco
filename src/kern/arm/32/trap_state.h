
#pragma once

#include "l4_types.h"
#include "entry_frame.h"
#include "processor.h"
#include "globalconfig.h"

#if ! defined(CONFIG_CPU_VIRT)
#include "processor.h"
#include "mem.h"
#endif // ! CONFIG_CPU_VIRT


class Trap_state_regs
{
public:
//  static int (*base_handler)(Trap_state *) asm ("BASE_TRAP_HANDLER");

  Mword pf_address;
  union
  {
    Mword error_code;
    Arm_esr esr;
  };

  Mword r[13];
};

class Trap_state : public Trap_state_regs, public Return_frame
{
public:
  typedef int (*Handler)(Trap_state*, Cpu_number cpu);
  bool exclude_logging() { return false; }

#if ! defined(CONFIG_CPU_VIRT)
  void
  copy_and_sanitize(Trap_state const *src)
  {
    // copy pf_address, esr, r0..r12, usp, ulr / omit km_lr
    Mem::memcpy_mwords(this, src, 17);
    pc = src->pc;
    psr = access_once(&src->psr);
    psr &= ~(Proc::Status_mode_mask | Proc::Status_interrupts_mask);
    psr |= Proc::Status_mode_user | Proc::Status_always_mask;
  }
#endif // ! CONFIG_CPU_VIRT

  void set_pagefault(Mword pfa, Mword error)
  {
    pf_address = pfa;
    error_code = error;
  }

  unsigned long trapno() const
  { return esr.ec(); }

  Mword error() const
  { return error_code; }

  bool exception_is_undef_insn() const
  { return esr.ec() == 0; }

  void dump() const;
};

struct Trex
{
  Trap_state s;
  Mword tpidruro;
  void set_ipc_upcall()
  { s.esr.ec() = 0x3f; }

  void dump() { s.dump(); }
};


