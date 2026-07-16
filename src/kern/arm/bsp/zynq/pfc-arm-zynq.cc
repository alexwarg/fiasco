
#include <pfc-arm.h>
#include <types.h>
#include <processor.h>
#include <mem.h>
#include <cpu.h>
#include <mmio_register_block.h>
#include <ipi.h>
#include <kmem_mmio.h>
#include <infinite_loop.h>

namespace {

struct Pfc_z : Pfc_arm
{
  bool do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    Mmio_register_block cpu1boot(Kmem_mmio::map(0xfffffff0, sizeof(Mword)));
    cpu1boot.write<Mword>(phys_tramp_mp_addr, 0);
    Mem::mp_wmb();
    asm volatile("sev");
    return true;
  }

  [[noreturn]] void system_reboot() override
  {
    Mmio_register_block slcr(Kmem_mmio::map(0xf8000000, 0x1000));
    slcr.write<Unsigned32>(0xdf0d, 0x8);
    slcr.write<Unsigned32>(1, 0x200);

    L4::infinite_loop();
  }
};

static Pfc_singleton<Pfc_z> __pfc;

}

