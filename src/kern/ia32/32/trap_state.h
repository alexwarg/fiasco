
#pragma once

#include "l4_types.h"
#include "regdefs.h"
#include "gdt.h"
#include "mem.h"

#include <cxx/atomic>

class Trap_state
{
  friend class Jdb_tcb;
  friend class Jdb_stack_view;

public:
  typedef FIASCO_FASTCALL int (*Handler)(Trap_state*, Cpu_number cpu);
  static Handler base_handler asm ("BASE_TRAP_HANDLER");

  // Saved segment registers
  Mword  _es;
  Mword  _ds;
  Mword  _gs;                                     // => utcb->values[ 0]
  Mword  _fs;                                     // => utcb->values[ 1]

  // PUSHA register state frame
  Mword  _di;                                     // => utcb->values[ 2]
  Mword  _si;                                     // => utcb->values[ 3]
  Mword  _bp;                                     // => utcb->values[ 4]
  Mword  _cr2; // we save cr2 over esp for PFs    // => utcb->values[ 5]
  Mword  _bx;                                     // => utcb->values[ 6]
  Mword  _dx;                                     // => utcb->values[ 7]
  Mword  _cx;                                     // => utcb->values[ 8]
  Mword  _ax;                                     // => utcb->values[ 9]

  // Processor trap number, 0-31
  Mword  _trapno;                                 // => utcb->values[10]

  // Error code pushed by the processor, 0 if none
  Mword  _err;                                    // => utcb->values[11]

  // Processor state frame
  Mword  _ip;                                     // => utcb->values[12]
  Mword  _cs;                                     // => utcb->values[13]
  Mword  _flags;                                  // => utcb->values[14]
  Mword  _sp;                                     // => utcb->values[15]
  Mword  _ss;

  void sanitize_user_state()
  {
    _cs = Gdt::gdt_code_user | Gdt::Selector_user;
    _ss = Gdt::gdt_data_user | Gdt::Selector_user;
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

  Mword dx() const
  { return _dx; }

  Mword value3() const
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
  {
    Address n = _ip;
    cxx::atomic_compare_exchange_strong((Address*)(&_ip), n, n + count, cxx::memory_order_relaxed);
  }

  void dump() const;

  bool exclude_logging() const noexcept
  { return _trapno == 1 || _trapno == 3; }
};

struct Trex
{
  Trap_state s;

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

