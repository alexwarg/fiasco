
#pragma once

#include "entry_frame.h"

class Trap_state : public Entry_frame
{
public:
  typedef int (*Handler)(Trap_state*, Cpu_number cpu);

  Mword trapno() const { return esr.ec(); }
  Mword error() const { return esr.raw(); }

  void set_pagefault(Mword pfa, Mword esr)
  {
    this->pf_address = pfa;
    this->esr = Arm_esr(esr);
  }

  bool exclude_logging() { return false; }

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

