#pragma once

#include <regdefs.h>
#include <types.h>

namespace PF
{
inline Mword is_translation_error(Mword error)
{
  return !(error & PF_ERR_PRESENT);
}

inline Mword is_usermode_error(Mword error)
{
  return (error & PF_ERR_USERMODE);
}

inline Mword is_read_error(Mword error)
{
  return !(error & PF_ERR_WRITE);
}

inline Mword addr_to_msgword0(Address pfa, Mword error)
{
  Mword v = (pfa & ~0x7) | (error & (PF_ERR_PRESENT | PF_ERR_WRITE));
  if (error & PF_ERR_INSTFETCH)
    v |= 0x4;
  return v;
}

}
