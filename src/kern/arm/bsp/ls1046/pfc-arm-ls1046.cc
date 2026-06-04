
#include <pfc-psci.h>

namespace {

struct Pfc_z : Pfc_psci
{
  void do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    boot_ap_cpus_psci(phys_tramp_mp_addr, { 0x0, 0x1, 0x2, 0x3 });
  }
};

static Pfc_singleton<Pfc_z> __pfc;
}
