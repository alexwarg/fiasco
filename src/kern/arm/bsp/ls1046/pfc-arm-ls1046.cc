
#include <pfc-psci.h>
#include <types.h>
#include <minmax.h>
#include <cstdio>
#include <processor.h>
#include <mem.h>
#include <psci.h>
#include <cpu.h>

namespace {

struct Pfc_z : Pfc_psci
{
  void do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    int seq = 1;
    for (unsigned i = 0; i < min<unsigned>(4, Config::Max_num_cpus); ++i)
      {
        unsigned coreid[] = { 0x0, 0x1, 0x2, 0x3 };
        int r = Psci::cpu_on(coreid[i], phys_tramp_mp_addr);
        if (r)
          {
            if (r != Psci::Psci_already_on)
              printf("CPU%d boot-up error: %d\n", i, r);
            continue;
          }

        while (!Cpu::online(Cpu_number(seq)))
          {
            Mem::barrier();
            Proc::pause();
          }
        ++seq;
      }
  }
};

static Pfc_singleton<Pfc_z> __pfc;
}
