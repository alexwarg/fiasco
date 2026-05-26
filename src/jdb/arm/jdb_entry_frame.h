
#pragma once

#include <cpu.h>
#include <trap_state.h>
#include <tb_entry.h>
#include <processor.h>

#include <globalconfig.h>

class Jdb_entry_frame : public Trap_state
{
public:
  Address_type from_user() const
  { return check_valid_user_psr() ? ADDR_USER : ADDR_KERNEL; }

  Address ksp() const
  { return Address(this); }

  Address ip() const
  { return pc; }

  Mword param() const
  { return r[0]; }

  char const *text() const
  { return reinterpret_cast<char const *>(r[0]); }

  unsigned textlen() const
  { return r[1]; }

#ifdef CONFIG_BIT32
  // Error = 0x33UL: see DEBUGGER_ENTRY
  bool debug_entry_kernel_str() const
  { return error_code == (0x33UL << 26); }

  bool debug_entry_user_str() const
  { return error_code == ((0x33UL << 26) | 1); }

  bool debug_entry_kernel_sequence() const
  { return error_code == ((0x33UL << 26) | 2); }

  bool debug_ipi() const
  { return error_code == ((0x33UL << 26) | 3); }
#endif
#ifdef CONFIG_BIT64
  // Error 0x3cUL: 'brk'
  bool debug_entry_kernel_str() const
  { return error_code == ((0x3cUL << 26) | (1 << 25)); }

  bool debug_entry_user_str() const
  { return error_code == ((0x3cUL << 26) | (1 << 25) | 1); }

  bool debug_entry_kernel_sequence() const
  { return error_code == ((0x3cUL << 26) | (1 << 25) | 2); }

  bool debug_ipi() const
  { return error_code == ((0x3cUL << 26) | (1 << 25) | 3); }
#endif
};
