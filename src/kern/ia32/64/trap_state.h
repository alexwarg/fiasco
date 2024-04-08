
#pragma once

#include "l4_types.h"
#include "gdt.h"
#include "regdefs.h"
#include "mem.h"

#include <cxx/atomic>

class Trap_state
{
  friend class Jdb_tcb;
  friend class Jdb_stack_view;
public:
  typedef FIASCO_FASTCALL int (*Handler)(Trap_state*, Cpu_number cpu);
  static Handler base_handler asm ("BASE_TRAP_HANDLER");

  // No saved segment registers

  // register state frame
  Mword  _r15;
  Mword  _r14;
  Mword  _r13;
  Mword  _r12;
  Mword  _r11;
  Mword  _r10;
  Mword  _r9;
  Mword  _r8;
  Mword  _di;
  Mword  _si;
  Mword  _bp;
  Mword  _cr2;  // we save cr2 over esp for page faults
  Mword  _bx;
  Mword  _dx;
  Mword  _cx;
  Mword  _ax;

  // Processor trap number, 0-31
  Mword  _trapno;

  // Error code pushed by the processor, 0 if none
  Mword  _err;

  // Processor state frame
  Mword  _ip;
  Mword  _cs;
  Mword  _flags;
  Mword  _sp;
  Mword  _ss;

  void sanitize_user_state()
  {
    if (EXPECT_FALSE(   (_cs != (Gdt::gdt_code_user | Gdt::Selector_user))
                     && (_cs != (Gdt::gdt_code_user32 | Gdt::Selector_user))))
      _cs = Gdt::gdt_code_user | Gdt::Selector_user;
    _flags = (_flags & ~(EFLAGS_IOPL | EFLAGS_NT)) | EFLAGS_IF;
  }

  void copy_and_sanitize(Trap_state const *src)
  {
    Mem::memcpy_mwords(this, src, sizeof(*this) / sizeof(Mword));
    sanitize_user_state();
  }

  void set_pagefault(Mword pfa, Mword error)
  {
    _cr2 = pfa;
    _trapno = 0xe;
    _err = error;
  }

  Mword trapno() const
  { return _trapno; }

  Mword error() const
  { return _err; }

  Mword ip() const
  { return _ip; }

  Mword cs() const
  { return _cs; }

  Mword flags() const
  { return _flags; }

  Mword sp() const
  { return _sp; }

  Mword ss() const
  { return _ss; }

  Mword value() const
  { return _ax; }

  Mword value2() const
  { return _cx; }

  Mword value3() const
  { return _dx; }

  Mword dx() const
  { return _dx; }

  Mword value4() const
  { return _bx; }

  void ip(Mword ip)
  { _ip = ip; }

  void cs(Mword cs)
  { _cs = cs; }

  void flags(Mword flags)
  { _flags = flags; }

  void sp(Mword sp)
  { _sp = sp; }

  void ss(Mword ss)
  { _ss = ss; }

  void value(Mword value)
  { _ax = value; }

  void value3(Mword value)
  { _dx = value; }

  void consume_instruction(unsigned count)
  { cxx::atomic_compare_exchange_strong((Address*)(&_ip), _ip, _ip + count); }

  void dump() const;

  bool exclude_logging() const noexcept
  { return _trapno == 1 || _trapno == 3; }
};

struct Trex
{
  Trap_state s;
  Mword fs_base;
  Mword gs_base;
  Unsigned16 ds, es, fs, gs;

  void set_ipc_upcall()
  {
    s._err = 0;
    s._trapno = 0xfe;
  }

  void dump() { s.dump(); }
};

namespace Ts
{
  enum
  {
    /// full number of words in a Trap_state
    Words = sizeof(Trap_state) / sizeof(Mword),
    /// words for the IRET frame at the end of the trap state
    Iret_words = 5,
    /// words for error code and trap number
    Code_words = 2,
    /// offset of the IRET frame
    Iret_offset = Words - Iret_words,
    /// number of words used for normal registers
    Reg_words = Words - Iret_words - Code_words,
  };
}

