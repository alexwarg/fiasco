#include <pfc-mips.h>
#include <infinite_loop.h>
#include <kmem.h>
#include <mmio_register_block.h>

namespace {

struct Pfc_pf : Pfc_mips
{
  [[noreturn]] void system_reboot() override
  {
    Register_block<32> wdg(Kmem::mmio_remap(0x18102100, sizeof(Unsigned32)));
    wdg[0] = 1;
    L4::infinite_loop();
  }
};

static Pfc_singleton<Pfc_pf> __pfc;
}
