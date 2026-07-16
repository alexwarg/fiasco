
#include <pfc-psci.h>

namespace {

struct Pfc_z : Pfc_psci
{
  bool do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    boot_ap_cpus_psci(phys_tramp_mp_addr,
                      { 0x000, 0x001,
                        0x100, 0x101,
                        0x200, 0x201,
                        0x300, 0x301,
                        0x400, 0x401,
                        0x500, 0x501,
                        0x600, 0x601,
                        0x700, 0x701 });
    return true;
  }
};

static Pfc_singleton<Pfc_z> __pfc;

}
