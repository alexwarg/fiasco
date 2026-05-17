#include <pfc-mips.h>
#include <infinite_loop.h>
#include <kmem.h>
#include <mmio_register_block.h>

#include <globalconfig.h>

#ifdef CONFIG_MP
#include <ipi.h>
#include <cm.h>

extern char _tramp_mp_entry[];

#endif

namespace {

struct Pfc_pf : Pfc_mips
{
  [[noreturn]] void system_reboot() override
  {
    enum
    {
      SOFTRES_REGISTER = 0x1f000500,
      GORESET          = 0x42,
    };

    Register_block<32> r(Kmem::mmio_remap(SOFTRES_REGISTER, sizeof(Unsigned32)));
    r[0] = GORESET;

    L4::infinite_loop();
  }

#ifdef CONFIG_MP
  void boot_ap_cpus() override
  {
    // no IPIs supported cannot boot other CPUs
    if (Ipi::hw == nullptr)
      return;

    if (Cm::present()
        && Cm::cm->cpc_present())
      {
        Pfc_mips::boot_ap_cpus();
        return;
      }

    // AMON based secondary CPU startup
    struct Cpu_launch
    {
      unsigned long pc;
      unsigned long gp;
      unsigned long sp;
      unsigned long a0;
      unsigned long _pad[3]; /* pad to cache line size to avoid thrashing */
      unsigned long flags;
    };

    Cpu_launch volatile *cl = reinterpret_cast<Cpu_launch volatile *>(0x80000f00);

    for (unsigned i = 0; i < 8; ++i)
      {
        if (!(cl[i].flags & 1))
          continue;

        cl[i].pc = reinterpret_cast<Address>(&_tramp_mp_entry);
        asm volatile ("sync" : : : "memory");
        cl[i].flags |= 2;
      }
  }
#endif
};

static Pfc_singleton<Pfc_pf> __pfc;
}
