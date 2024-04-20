#pragma once

#include <trap_state.h>
#include <types.h>

namespace PF
{
inline Mword is_usermode_error(Mword error)
{
  return (error & Trap_state::C_src_context_mask) == Trap_state::C_src_user;
}

inline Mword is_read_error(Mword cause)
{
  // bit 0 in the exception code denotes a write / store access
  // in all TLB, Address, and bus errors
  // 0x13 is triggered on a read access if the RI (read inhibit) bit is set.
  return !(cause & 4) || ((cause & 0x7c) == (0x13 << 2));
}

inline Mword is_read_error(Trap_state::Cause const cause)
{ return is_read_error(cause.raw); }

inline Mword is_xi_error(Mword cause)
{
  // TLBXI
  auto code = cause & 0x7c;
  return code == (0x14 << 2);
}

inline Mword is_tlb_rights_error(Mword cause)
{
  auto code = cause & 0x7c;
  return code == (0x14 << 2) || code == (0x13 << 2);
}

inline Mword is_translation_error(Trap_state::Cause const cause)
{
  return    cause.exc_code() == 2  // TLBL
         || cause.exc_code() == 3; // TLBS
}

inline Mword is_translation_error(Mword cause)
{
  return is_translation_error(Trap_state::Cause(cause));
}

inline Mword addr_to_msgword0(Address pfa, Mword cause)
{
  Mword a = pfa & ~7;
  if (is_translation_error(cause))
    a |= 1; // NOT present

  if(!is_read_error(cause))
    a |= 2;

  if (is_xi_error(cause))
    {
      // Executing non-executable page.
      a |= 4;
    }

  return a;
}

}
