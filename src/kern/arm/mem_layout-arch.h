#pragma once

#include <globalconfig.h>
#include "config.h"
#include "types.h"
#include <mem_layout-arm-bits.h>
#include <mem_layout_arm_bsp.h>
#include <paging_bits.h>

class Mem_layout_arch : public Mem_layout_arm_bits, public Mem_layout_arm_bsp
{
public:
  enum Phys_layout : Address {
    Sdram_phys_base = RAM_PHYS_BASE,
  };

private:
  // At least two entries are expected: the kernel image and the heap. If the
  // RAM is not contiguous there might be more than one heap region needed,
  // though.
  enum { Max_pmem_regions = 4 };

  struct Pmem_region
  {
    Address paddr;
    Address vaddr;
    unsigned long size;
  };

  static Pmem_region _pm_regions[Max_pmem_regions];
  static unsigned _num_pm_regions;

public:
  static Address phys_to_pmem(Address phys)
  {
    for (unsigned i = 0; i < Max_pmem_regions && i < _num_pm_regions; ++i)
      {
        if (   phys >= _pm_regions[i].paddr
            && phys <= _pm_regions[i].paddr + _pm_regions[i].size)
          return phys - _pm_regions[i].paddr + _pm_regions[i].vaddr;
      }

    return ~0UL;
  }

  static Address pmem_to_phys(Address virt)
  {
    for (unsigned i = 0; i < Max_pmem_regions && i < _num_pm_regions; ++i)
      {
        if (   virt >= _pm_regions[i].vaddr
            && virt <= _pm_regions[i].vaddr + _pm_regions[i].size)
          return virt - _pm_regions[i].vaddr + _pm_regions[i].paddr;
      }

    return ~0UL;
  }

  static inline Address pmem_to_phys(void const *addr)
  { return pmem_to_phys(Address(addr)); }

  static bool add_pmem(Address phys, Address virt, unsigned long size);


#ifdef CONFIG_VIRT_OBJ_SPACE
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
#endif
};
