
#include <pfc-arm.h>
#include <pfc-psci.h>
#include <types.h>
#include <processor.h>
#include <mem.h>
#include <cpu.h>

#include <cstdio>

namespace {

struct Pfc_z : Pfc_psci
{
  void do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    for (int i = 0; i < min<unsigned>(4, Config::Max_num_cpus); ++i)
      {
        unsigned coreid[4] = { 0x0, 0x1, 0x2, 0x3 };
        int r = cpu_on(coreid[i], phys_tramp_mp_addr);
        if (r)
          {
            if (r != Psci::Psci_already_on)
              printf("CPU%d boot-up error: %d\n", i, r);
            continue;
          }

        // The Zynq-MP firmware will not boot all CPUs if we fire CPU_ON
        // events too fast, thus wait for each CPU to appear so that we do not
        // overburden the firmware.
        while (!Cpu::online(Cpu_number(i)))
          {
            Mem::barrier();
            Proc::pause();
          }
      }
  }
};

static Pfc_singleton<Pfc_z> __pfc;

}
