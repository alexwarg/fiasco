#include <pfc-psci.h>

namespace {

struct Pfc_armada37xx : Pfc_psci
{
  bool do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    boot_ap_cpus_psci(phys_tramp_mp_addr, { 0, 1 });
    return true;
  }
};

static Pfc_singleton<Pfc_armada37xx> __pfc;

}
