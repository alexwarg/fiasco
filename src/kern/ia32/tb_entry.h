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
  { return Cpu::rdtsc(); }
};


#include "tb_entry_generic.h"

/** logged trap. */
class Tb_entry_trap : public Tb_entry
{
private:
  Unsigned8	_trapno;
  Unsigned16	_error;
  Mword	_bp, _cr2, _ax, _flags, _sp;
  Unsigned16	_cs,  _ds;
public:
  void print(String_buffer *buf) const;

  void set(Mword ip, Trap_state *ts)
  {
    _ip     = ip;
    _trapno = ts->_trapno;
    _error  = ts->_err;
    _cr2    = ts->_cr2;
    _ax     = ts->_ax;
    _cs     = (Unsigned16)ts->cs();
#if defined (CONFIG_32BIT)
    _ds     = (Unsigned16)ts->_ds;
#endif // CONFIG_32BIT
    _sp     = ts->sp();
    _flags  = ts->flags();
  }

  void set(Mword eip, Mword trapno)
  {
    _ip = eip;
#if defined (CONFIG_32BIT)
    _trapno = trapno;
    _cs     = 0;
#else  // CONFIG_32BIT
    _trapno = trapno | 0x80;
#endif // CONFIG_32BIT
  }

  Unsigned8 trapno() const
  { return _trapno; }

  Unsigned16 error() const
  { return _error; }

  Mword eax() const
  { return _ax; }

  Mword cr2() const
  { return _cr2; }

  Mword ebp() const
  { return _bp; }

  Unsigned16 cs() const
  { return _cs; }

  Unsigned16 ds() const
  { return _ds; }

  Mword sp() const
  { return _sp; }

  Mword flags() const
  { return _flags; }

} __attribute__((packed));
