#pragma once

#include <l4_types.h>
#include <types.h>
#include <globalconfig.h>

class Exynos_smc
{
public:
#if defined (CONFIG_ARM_EM_NS) && defined (CONFIG_SECMONIF_MC)
public:
  enum Command : Smword
  {
    Init       = -1,
    Info       = -2,
    Sleep      = -3,
    Cpu1boot   = -4,
    Cpu0aftr   = -5,
    C15resume  = -11,
    L2x0ctrl   = -21,
    L2x0setup1 = -22,
    L2x0setup2 = -23,
    L2x0invall = -24,
    L2x0debug  = -25,
    Cp15_reg   = -101,
  };

  static int call(Command cmd, Mword a1 = 0, Mword a2 = 0, Mword a3 = 0)
  {
    register Mword _cmd asm("r0") = cmd;
    register Mword _a1  asm("r1") = a1;
    register Mword _a2  asm("r2") = a2;
    register Mword _a3  asm("r3") = a3;

    asm volatile("dmb   \t\n" // Errata 766421
                 "smc 0 \t\n"
                 "dmb   \t\n" // Errata 766421
                 : "=r" (_cmd)
                 : "r" (_cmd), "r" (_a1), "r" (_a2), "r" (_a3)
                 : "memory", "cc");
    return _cmd;
  }

  static void l2cache_setup(unsigned tag_lat, unsigned data_lat,
                            unsigned prefctrl, Mword setup2val,
                            Mword aux_val, Mword aux_mask)
  {
    call(L2x0setup1, tag_lat, data_lat, prefctrl);
    call(L2x0setup2, setup2val, aux_val, aux_mask);
    call(L2x0invall);
    call(L2x0ctrl, 1);
  }

  static void write_cp15(unsigned opc1, unsigned crn,
                         unsigned crm, unsigned opc2,
                         Mword val)
  {
    call(Cp15_reg, cp15_cmd(opc1, crn, crm, opc2), val);
  }
#endif // CONFIG_ARM_EM_NS && CONFIG_SECMONIF_MC
#if ! defined (CONFIG_ARM_EM_NS) || defined (CONFIG_SECMONIF_NONE)
  static void l2cache_setup(unsigned, unsigned, unsigned, Mword, Mword, Mword)
  {}

  static void write_cp15(unsigned, unsigned, unsigned, unsigned, Mword)
  {}
#endif // ! CONFIG_ARM_EM_NS || CONFIG_SECMONIF_NONE
};


