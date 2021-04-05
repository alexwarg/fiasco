
#include <pfc-arm.h>
#include <pfc-psci.h>
#include <types.h>
#include <processor.h>
#include <mem.h>
#include <cpu.h>

#include <cstdio>

namespace {

struct Pfc_a8k : Pfc_psci
{
  void do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    unsigned coreid[4] = { 0x000, 0x001, 0x100, 0x101 };
    for (int i = 0; i < min<int>(4, Config::Max_num_cpus); ++i)
      {
        int r = cpu_on(coreid[i], phys_tramp_mp_addr);
        if (r)
          {
            if (r != Psci::Psci_already_on)
              printf("CPU%d boot-up error: %d\n", i, r);
            continue;
          }

        while (!Cpu::online(Cpu_number(i)))
          {
            Mem::barrier();
            Proc::pause();
          }
      }
  }
};

static Pfc_singleton<Pfc_a8k> __pfc;

}
