#pragma once

#include <globalconfig.h>
#include "config.h"
#include "types.h"
#include <mem_layout-arm-bits.h>
#include <mem_layout_arm_bsp.h>

class Mem_layout_arch : public Mem_layout_arm_bits, public Mem_layout_arm_bsp
{
public:
  enum Phys_layout : Address {
    Sdram_phys_base = RAM_PHYS_BASE
  };

  static Mword _read_special_safe(Mword const *a);
  static bool _read_special_safe(Mword const *address, Mword &v);

  template<typename V>
  static inline bool read_special_safe(V const *address, V &v)
  {
    Mword _v;
    bool ret = _read_special_safe(reinterpret_cast<Mword const*>(address), _v);
    v = V(_v);
    return ret;
  }

  template<typename T>
  static inline T read_special_safe(T const *a)
  { return T(_read_special_safe((Mword const *)a)); }

  static Address pmem_to_phys(Address addr);
  static inline Address pmem_to_phys(void const *addr)
  { return pmem_to_phys(Address(addr)); }

#ifdef CONFIG_NONCONT_MEM
  static inline Address phys_to_pmem(Address phys)
  {
    Address virt = ((unsigned long)__ph_to_pm[phys >> Config::SUPERPAGE_SHIFT]) << 16;
    if (!virt)
      return ~0UL;
    return virt | (phys & (Config::SUPERPAGE_SIZE - 1));
  }

  static inline ALWAYS_INLINE void add_pmem(Address phys, Address virt, unsigned long size)
  {
    for (; size >= Config::SUPERPAGE_SIZE; size -= Config::SUPERPAGE_SIZE)
      {
        __ph_to_pm[phys >> Config::SUPERPAGE_SHIFT] = virt >> 16;
        phys += Config::SUPERPAGE_SIZE;
        virt += Config::SUPERPAGE_SIZE;
      }
  }

private:
  static unsigned short __ph_to_pm[1UL << (32 - Config::SUPERPAGE_SHIFT)];
public:
#else
  static inline Address phys_to_pmem(Address addr)
  { return addr - Sdram_phys_base + Map_base; }
#endif
};
