
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
    int seq = 1;
    for (unsigned i = 0; i < min<unsigned>(16, Config::Max_num_cpus); ++i)
      {
        unsigned coreid[] = {   0x0, 0x001,
                              0x100, 0x101,
                              0x200, 0x201,
                              0x300, 0x301,
                              0x400, 0x401,
                              0x500, 0x501,
                              0x600, 0x601,
                              0x700, 0x701 };
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
