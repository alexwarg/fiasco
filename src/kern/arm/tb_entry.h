
#pragma once

#include "trap_state.h"
#include "globalconfig.h"
#include "pre_parts.h"

struct Tb_entry_arch
{
  enum
  {
    Tb_entry_size = 16 * sizeof(Mword),
  };

  static Unsigned64 read_cycle_counter();
};

#if defined (PRE_arm_generic_timer)

#include "generic_timer.h"

inline Unsigned64
Tb_entry_arch::read_cycle_counter()
{
  return Generic_timer::Gtimer::counter();
}

#else  // PRE_arm_generic_timer

inline Unsigned64
Tb_entry_arch::read_cycle_counter()
{
  return 0; // dummy
}

#endif // PRE_arm_generic_timer


#include "tb_entry_generic.h"


/** logged trap. */
class Tb_entry_trap : public Tb_entry
{
private:
  Unsigned32    _error;
  Mword         _cpsr, _sp;
public:
  void print(String_buffer *buf) const;

  Unsigned16 cs() const
  { return 0; }

  Unsigned8 trapno() const
  { return 0; }

  Unsigned32 error() const
  { return _error; }

  Mword sp() const
  { return _sp; }

  Mword cr2() const
  { return 0; }

  Mword eax() const
  { return 0; }

  void set(Mword ip, Trap_state *ts)
  {
    _ip    = ip;
    _error = ts->error_code;
    _cpsr  = ts->psr;
    _sp    = ts->sp();
  }

  void set(Mword pc, Mword)
  {
    _ip    = pc;
  }
};


