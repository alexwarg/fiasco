#include <pfc.h>
#include <pfc-psci.h>
#include <globalconfig.h>

struct Pfc_arm_virt : Pfc_psci
{
#ifdef CONFIG_MP
  void do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    for (unsigned i = 0; i < Config::Max_num_cpus; ++i)
      cpu_on(((i & 0xf0) << 4) | (i & 0xf), phys_tramp_mp_addr);
  }
#endif
};

static Pfc_singleton<Pfc_arm_virt> __pfc;
