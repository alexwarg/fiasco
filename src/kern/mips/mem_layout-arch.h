#pragma once

#include <globalconfig.h>
#include "config.h"
#include "types.h"
#include <mem_layout-defaults.h>

class Mem_layout_arch : public Mem_layout_defaults<Mem_layout_arch>
{
public:
#ifdef CONFIG_CPU_MIPS32
  enum Virt_layout : Address
  {
    User_max             = 0x7fffffff,
    Utcb_addr            = User_max + 1UL - (Config::PAGE_SIZE * 4),
  };

  enum : Address
  {
    KSEG0  = 0x80000000,
    KSEG0e = 0x9fffffff,
    KSEG1  = 0xa0000000,
    KSEG1e = 0xbfffffff,

    Exception_base = 0x80000000,
  };

  static inline bool below_512mb(Address addr)
  { return !(addr & 0xe0000000); }

#else // mips64
  enum Virt_layout : Address
  {
    User_max             = 0x0000000100000000 - 1,
    Utcb_addr            = User_max + 1UL - (Config::PAGE_SIZE * 4),
  };

  enum : Address
  {
    KSEG0  = 0xffffffff80000000,
    KSEG0e = 0xffffffff9fffffff,
    KSEG1  = 0xffffffffa0000000,
    KSEG1e = 0xffffffffbfffffff,

    Exception_base = 0xffffffff80000000,
  };

  static inline bool below_512mb(Address addr)
  { return !(addr & 0xffffffffe0000000); }

#endif

  static inline Address phys_to_pmem(Address addr)
  { return addr + KSEG0; }

  static inline Address pmem_to_phys(Address addr)
  { return addr - KSEG0; }

  static inline Address pmem_to_phys(void const *ptr)
  { return pmem_to_phys((Address)ptr); }

  static inline bool is_user_space(Address addr)
  { return addr < KSEG0; }
};
