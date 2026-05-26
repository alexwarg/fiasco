#pragma once

#include <cp0_status.h>
#include <trap_state.h>
#include <tb_entry.h>
#include <processor.h>


class Jdb_entry_frame : public Trap_state
{
public:
  bool debug_entry_kernel_str() const
  {
    Cause c(cause);
    return c.exc_code() == 9 && r[Entry_frame::R_t0] == 0;
  }

  bool debug_entry_user_str() const
  {
    Cause c(cause);
    return c.exc_code() == 9 && r[Entry_frame::R_t0] == 1;
  }

  bool debug_entry_kernel_sequence() const
  {
    Cause c(cause);
    return c.exc_code() == 9 && r[Entry_frame::R_t0] == 2;
  }

  bool debug_ipi() const
  {
    Cause c(cause);
    return c.exc_code() == 9 && c.bp_spec() == 3;
  }

  Address_type from_user() const
  {
    return (status & Cp0_status::ST_KSU_USER) ? ADDR_USER : ADDR_KERNEL;
  }

  Address ksp() const
  { return Address(this); }

  Address ip() const
  { return epc; }

  char const *text() const
  { return reinterpret_cast<char const *>(r[Entry_frame::R_a0]); }

  unsigned textlen() const
  { return r[Entry_frame::R_a1]; }

  //---------------------------------------------------------------------------
  // the following register usage matches ABI in l4sys/include/ARCH-mips/kdebug.h
  Mword param() const
  { return r[Entry_frame::R_t1]; }
};

