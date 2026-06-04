#include <pfc-psci.h>

struct Pfc_arm_virt : Pfc_psci
{
  void do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    boot_ap_cpus_psci(phys_tramp_mp_addr,
                      { 0x000, 0x001, 0x002, 0x003,
                        0x100, 0x101, 0x102, 0x103 });
  }
};

static Pfc_singleton<Pfc_arm_virt> __pfc;
