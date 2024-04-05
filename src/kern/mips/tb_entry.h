#pragma once

#include "trap_state.h"
#include "globalconfig.h"
#include "cpu.h"

struct Tb_entry_arch
{
  enum
  {
    Tb_entry_size = 16 * sizeof(Mword),
  };

  static Unsigned64 read_cycle_counter()
  { return 0; }
};


#include "tb_entry_generic.h"

/** logged trap. */
class Tb_entry_trap : public Tb_entry
{
private:
  Unsigned32 _cause;
  Unsigned32 _status;
  Mword _sp;
public:
  void print(String_buffer *buf) const;

  Unsigned16 cs() const
  { return 0; }

  Unsigned8 trapno() const
  { return Trap_state::Cause(_cause).exc_code(); }

  Unsigned32 error() const
  { return _cause; }

  Mword sp() const
  { return _sp; }

  Mword cr2() const
  { return 0; }

  Mword eax() const
  { return 0; }

  void set(Mword pc, Trap_state *ts)
  {
    _ip = pc;
    _cause = ts->cause;
    _status  = ts->status;
  }

  void set(Mword pc, Mword cause)
  {
    _ip = pc;
    _cause = cause;
  }
};
