#pragma once

#include <types.h>

class Irq_mgr_dyn;

namespace Pic_gic
{
  struct Gic_info
  {
    unsigned short version;
    unsigned short primary;
    unsigned offset;

    Address dist_phys;
    Address dist_size;

    Address cpu_phys = 0;
    Address cpu_size = 0;

    Address redist_phys = 0;
    Address redist_size = 0;

    Address its_phys = 0;
    Address its_size = 0;
  };

  extern Gic_info const primary_gic_info;

  constexpr Gic_info
  gic_v2_info(Address dist_phys, Address cpu_phys)
  { return Gic_info{ 2, true, 0, dist_phys, 0x1000, cpu_phys, 0x100, 0, 0, 0, 0 }; }

  constexpr Gic_info
  gic_v3_info(Address dist_phys, Address redist_phys, Address redist_size,
              Address its_phys = 0, Address its_size = 0)
  { return Gic_info{ 3, true, 0, dist_phys, 0x10000, 0, 0, redist_phys, redist_size, its_phys, its_size }; }

  constexpr Gic_info
  gic_vx_info(Address dist_phys,  Address cpu_phys, Address redist_phys, Address redist_size,
              Address its_phys = 0, Address its_size = 0)
  { return Gic_info{ 0, true, 0, dist_phys, 0x10000, cpu_phys, 0x100, redist_phys, redist_size, its_phys, its_size }; }

  int add_gic(Irq_mgr_dyn *mgr, Gic_info const &inf);
  int add_gic(Gic_info const &inf = primary_gic_info);
}
