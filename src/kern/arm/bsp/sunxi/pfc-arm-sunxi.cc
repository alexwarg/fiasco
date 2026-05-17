
#include <pfc-arm.h>
#include <types.h>
#include <processor.h>
#include <mem.h>
#include <cpu.h>
#include <mmio_register_block.h>
#include <ipi.h>
#include <kmem.h>
#include <infinite_loop.h>

namespace {

struct Pfc_z : Pfc_arm
{
  void do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    enum {
      CPUx_base      = 0x40,
      CPUx_offset    = 0x40,
      CPUx_RST_CTRL  = 0,
      GENER_CTRL_REG = 0x184,
      PRIVATE_REG    = 0x1a4,
    };
    Mmio_register_block c(Kmem::mmio_remap(0x01c25c00, 0x400));
    c.write<Mword>(phys_tramp_mp_addr, 0x1a4);

    unsigned cpu = 1;
    c.write<Mword>(0, CPUx_base + CPUx_offset * cpu + CPUx_RST_CTRL);
    c.clear<Mword>(1 << cpu, GENER_CTRL_REG);
    c.write<Mword>(3, CPUx_base + CPUx_offset * cpu + CPUx_RST_CTRL);

    Ipi::bcast(Ipi::Global_request, Cpu_number::boot_cpu());
  }

  [[noreturn]] void system_reboot()
  {
    Address wdt = Kmem::mmio_remap(0x01c20c90, 0x10);
    Io::write<Unsigned32>(3, wdt + 4);
    Io::write<Unsigned32>(1, wdt + 0);

    L4::infinite_loop();
  }
};


static Pfc_singleton<Pfc_z> __pfc;

}
