
#include <pfc-psci.h>

namespace {

struct Pfc_a8k : Pfc_psci
{
  void do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    boot_ap_cpus_psci(phys_tramp_mp_addr, { 0x000, 0x001, 0x100, 0x101 });
  }
};

static Pfc_singleton<Pfc_a8k> __pfc;

}
