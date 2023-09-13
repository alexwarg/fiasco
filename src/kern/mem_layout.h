#pragma once

#include <globalconfig.h>
#include "l4_types.h"
#include "std_macros.h"
#include <mem_layout-arch.h>

class Kpdir;

class Mem_layout : public Mem_layout_arch
{
public:
  static const char load             asm ("_load");
  static const char image_start      asm ("_kernel_image_start");
  static const char start            asm ("_start");
  static const char end              asm ("_end");
  static const char ecode            asm ("_ecode");
  static const char etext            asm ("_etext");
  static const char data_start       asm ("_kernel_data_start");
  static const char edata            asm ("_edata");
  static const char initcall_start[] asm ("_initcall_start");
  static const char initcall_end[]   asm ("_initcall_end");

  static inline Mword in_kernel(Address a)
  { return a > User_max; }

  static inline ALWAYS_INLINE Mword in_kernel_code(Address a)
  { return a >= (Address)&start && a < (Address)&ecode; }

#ifdef CONFIG_VIRT_OBJ_SPACE
  static inline bool is_caps_area(Address a)
  { return (a >= Caps_start) && (a < Caps_end); }
#else
  static inline bool is_caps_area(Address)
  { return false; }
#endif
};
