#pragma once

#include "mmio_register_block.h"
#include "globalconfig.h"
#include "std_macros.h"


class Gic_cpu_v2
{
private:
  Mmio_register_block _cpu;

public:
  enum
  {
    GICC_CTRL         = 0x00,
    GICC_PMR          = 0x04,
    GICC_BPR          = 0x08,
    GICC_IAR          = 0x0c,
    GICC_EOIR         = 0x10,
    GICC_RPR          = 0x14,
    GICC_HPPIR        = 0x18,

    Size              = 0x2000,

    GICC_CTRL_ENABLE_GRP0    = 1 << 0,
    GICC_CTRL_ENABLE_GRP1    = 1 << 1,
    GICC_CTRL_ENABLE         = GICC_CTRL_ENABLE_GRP0,
    GICC_CTRL_FIQEn          = 1 << 3,

    Cpu_iar_intid_mask = 0x3ff,

    Cpu_prio_val      = 0xf0,
  };

  static constexpr bool Config_tz_sec = IS_ENABLED(CONFIG_ARM_EM_TZ);

  void pmr(unsigned prio)
  {
    _cpu.write<Unsigned32>(prio, GICC_PMR);
  }

  void enable()
  {
    _cpu.write<Unsigned32>(GICC_CTRL_ENABLE | (Config_tz_sec ? GICC_CTRL_FIQEn : 0),
                           GICC_CTRL);
    pmr(Cpu_prio_val);
  }

  void disable()
  {
    _cpu.write<Unsigned32>(0, GICC_CTRL);
  }

  explicit Gic_cpu_v2(Address cpu_base) noexcept
    : _cpu(cpu_base)
  {}

  void ack(Unsigned32 irq)
  {
    _cpu.write<Unsigned32>(irq, GICC_EOIR);
  }

  Unsigned32 iar()
  {
    return _cpu.read<Unsigned32>(GICC_IAR);
  }

  unsigned pmr()
  { return _cpu.read<Unsigned32>(GICC_PMR); }

};
