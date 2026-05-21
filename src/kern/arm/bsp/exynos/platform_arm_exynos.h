#pragma once

#include "types.h"
#include "globalconfig.h"

class Platform
{
public:
  struct Pkg_id
  {
    Mword mask;
    Mword val;
    unsigned soc;
    unsigned uart;
  };

  enum Soc_type
  {
    Soc_unknown = 0,
    Soc_4210,
    Soc_4412,
    Soc_5250,
    Soc_5410,
  };

  enum Gic_type
  {
    Int_gic, Ext_gic,
  };

#ifdef CONFIG_PF_EXYNOS_EXTGIC
  static constexpr Gic_type gic_type() { return Ext_gic; }
#else
  static constexpr Gic_type gic_type() { return Int_gic; }
#endif
  static constexpr bool gic_ext() { return gic_type() == Ext_gic; }
  static constexpr bool gic_int() { return gic_type() == Int_gic; }

  static Soc_type soc_type()
  { type(); return _soc; }

  static unsigned subrev();
  static unsigned uart_nr();

  static bool is_4210();
  static bool is_4412();
  static bool is_5250();
  static bool is_5410();

private:
  static Soc_type _soc;
  static unsigned _uart;
  static unsigned _subrev;

  static void type();
  static void process_pkg_ids();
};
