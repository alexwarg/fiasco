#include <pfc-psci.h>
#include <cstdio>

namespace {

struct Pfc_armada37xx : Pfc_psci
{
  void do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    for (int i = 1; i < 2; ++i)
      {
        int r;
        static unsigned const coreid[2] = { 0, 1 };
        if ((r = cpu_on(coreid[i], phys_tramp_mp_addr)))
          printf("KERNEL: PSCI CPU_ON for CPU%d failed: %d\n", i, r);
      }
  }
};

static Pfc_singleton<Pfc_armada37xx> __pfc;

}
