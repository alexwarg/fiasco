#pragma once

#include <scu.h>
#include <cpu.h>
#include <kmem.h>
#include <globalconfig.h>

inline void setup_arm_scu(Address phys)
{
  Unsigned32 id = Cpu::midr() & 0xff00fff0;
  switch (id)
    {
    default:
      return;

    case 0x4100c090: // cortex A9
    case 0x4100c050: // cortex A5
    case 0x4100b020: // ARM11 MPCORE
      break;
    }
  printf("SCU init @%lx\n", phys);
#if defined (CONFIG_ARM_CORTEX_A9) || defined (CONFIG_ARM_MPCORE) || defined (CONFIG_ARM_CORTEX_A5)
#ifdef CONFIG_MP
  extern volatile Mword _tramp_mp_scu_phys;
  _tramp_mp_scu_phys = phys;
#endif
  Cpu::scu.r.set_mmio_base(Kmem::mmio_remap(phys, 0x1000));
  Cpu::scu.init(0);
#endif
}

