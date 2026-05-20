#pragma once

#include <types.h>
#include <pic-gic-helper.h>
#include <cxx/static_vector>

struct Rv_pf
{
  Address scu = 0;
  Address sys_r = 0;
  Address sys_c = 0;
  Address l2cxx0 = 0;
  Address sp804 = 0;

  bool syscon_gic = false;
  unsigned char n_gics;
  Pic_gic::Gic_info const gics[];
  using Gic_vect = cxx::static_vector<Pic_gic::Gic_info const>;

  constexpr Gic_vect
  g() const noexcept { return Gic_vect(gics, n_gics); }
};

Rv_pf const *rv_current_platform();

