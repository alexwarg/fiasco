#pragma once

#include <types.h>
#include <globalconfig.h>

namespace PF
{
#ifdef CONFIG_ARM_V5
inline Mword is_alignment_error(Mword error)
{
  return ((error >> 26) & 0x04) && ((error & 0x0d) == 0x001);
}
#endif // CONFIG_ARM_V5
#ifdef CONFIG_ARM_LPAE
inline Mword is_alignment_error(Mword error)
{
  return ((error >> 26) == 0x24) && ((error & 0x3f) == 0x21);
}

inline Mword is_translation_error(Mword error)
{
  return (error & 0x3c) == 0x04;
}

#else // CONFIG_ARM_LPAE
#ifdef CONFIG_ARM_V6PLUS
inline Mword is_alignment_error(Mword error)
{
  return ((error >> 26) == 0x24) && ((error & 0x40f) == 0x001);
}
#endif // CONFIG_ARM_V6PLUS
inline Mword is_translation_error(Mword error)
{
  return (error & 0x0d/*FSR_STATUS_MASK*/) == 0x05/*FSR_TRANSL*/;
}
#endif // CONFIG_ARM_LPAE

inline Mword is_usermode_error(Mword error)
{
  return !((error >> 26) & 1);
}

inline Mword is_read_error(Mword error)
{
  return !(error & (1 << 6));
}

inline Mword addr_to_msgword0(Address pfa, Mword error)
{
  Mword a = pfa & ~7;
  if (is_translation_error(error))
    a |= 1;
  if (!is_read_error(error))
    a |= 2;
  if (!((error >> 26) & 0x04))
    a |= 4;
  return a;
}

}
