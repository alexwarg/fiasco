#include <pfc-dt.h>

struct Pfc_arm_virt : Pfc_dt
{
  bool do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    if (Pfc_dt::do_boot_ap_cpus(phys_tramp_mp_addr))
      return true;
    boot_ap_cpus_psci(phys_tramp_mp_addr,
                      { 0x000, 0x001, 0x002, 0x003,
                        0x100, 0x101, 0x102, 0x103 });
    return true;
  }
};

static Pfc_singleton<Pfc_arm_virt> __pfc;
