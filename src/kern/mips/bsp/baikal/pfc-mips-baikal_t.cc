#include <pfc-mips.h>
#include <infinite_loop.h>
#include <kmem.h>
#include <mmio_register_block.h>

namespace {

struct Pfc_pf : Pfc_mips
{
  [[noreturn]] void system_reboot() override
  {
    Register_block<32> r(Kmem::mmio_remap(0x1f04c000, 0x10));
    r[0x0] = r[0] & ~3;
    r[0x4] = 0;
    r[0x0] = r[0] | 1;
    r[0xc] = 0x76;

    L4::infinite_loop();
  }
};

static Pfc_singleton<Pfc_pf> __pfc;
}
