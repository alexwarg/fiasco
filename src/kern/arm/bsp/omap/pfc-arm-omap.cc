
#include <pfc-arm.h>
#include <infinite_loop.h>
#include <io.h>
#include <kmem_mmio.h>
#include <mem_layout.h>
#include <mmio_register_block.h>
#include <ipi.h>
#include <globalconfig.h>

#if defined (CONFIG_PF_OMAP3_OMAP35XEVM) || defined(CONFIG_PF_OMAP3_BEAGLEBOARD)
struct Pfc_omap_35x : Pfc_arm
{
  [[noreturn]] void system_reboot() override
  {
    enum
    {
      PRM_RSTCTRL = Mem_layout::Prm_global_reg_phys_base + 0x50,
    };

    Io::write<Mword>(2, Kmem_mmio::remap(PRM_RSTCTRL, sizeof(Mword)));

    L4::infinite_loop();
  }
};
using Pfc_omap = Pfc_omap_35x;
#endif

#ifdef CONFIG_PF_OMAP3_AM33XX
struct Pfc_omap_am33xx : Pfc_arm
{
  [[noreturn]] void system_reboot() override
  {
    enum { PRM_RSTCTRL = 0x44e00F00, };
    Io::write<Mword>(1, Kmem_mmio::remap(PRM_RSTCTRL, sizeof(Mword)));
    L4::infinite_loop();
  }
};
using Pfc_omap = Pfc_omap_am33xx;
#endif

inline void omap_aux_smc(unsigned cmd, Mword arg0, Mword arg1)
{
  register unsigned long r0  asm("r0")  = arg0;
  register unsigned long r1  asm("r1")  = arg1;
  register unsigned long r12 asm("r12") = cmd;

  asm volatile(".arch_extension sec\n"
               "dsb                \n"
               "push {r11}         \n"
               "smc #0             \n"
               "pop {r11}          \n"
               : : "r" (r0), "r" (r1), "r" (r12)
               : "r2", "r3", "r4", "r5", "r6",
                 "r7", "r8", "r9", "r10", "lr", "memory");
}

struct Pfc_omap_4_5 : Pfc_arm
{
  virtual void setup_ap_boot(Mmio_register_block *aux, unsigned reg) = 0;

#ifdef CONFIG_MP
  bool do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    // omap4: two possibilities available, the memory mapped only in later
    // board revisions
    if (0)
      {
        enum {
          AUX_CORE_BOOT_0 = 0x104,
          AUX_CORE_BOOT_1 = 0x105,
        };
        omap_aux_smc(AUX_CORE_BOOT_1, phys_tramp_mp_addr, 0);
        asm volatile("dsb; sev" : : : "memory");
        omap_aux_smc(AUX_CORE_BOOT_0, 0x200, 0xfffffdff);
      }
    else
      {
        enum {
          AUX_CORE_BOOT_0 = 0,
          AUX_CORE_BOOT_1 = 4,
        };

        Mmio_register_block aux(Kmem_mmio::map(0x48281800, 0x800));
        aux.write<Mword>(phys_tramp_mp_addr, AUX_CORE_BOOT_1);
        setup_ap_boot(&aux, AUX_CORE_BOOT_0);
        asm volatile("dsb; sev" : : : "memory");
        Ipi::bcast(Ipi::Global_request, current_cpu());
      }
    return true;
  }
#endif
};

#ifdef CONFIG_PF_OMAP4_PANDABOARD
struct Pfc_omap_4 : Pfc_omap_4_5
{
  [[noreturn]] void system_reboot() override
  {
    enum
    {
      DEVICE_PRM  = Mem_layout::Prm_phys_base + 0x1b00,
      PRM_RSTCTRL = DEVICE_PRM + 0,
    };
    Address p = Kmem_mmio::remap(PRM_RSTCTRL, sizeof(Mword));

    Io::set<Mword>(1, p);
    Io::read<Mword>(p);

    L4::infinite_loop();
  }

  void setup_ap_boot(Mmio_register_block *aux, unsigned reg) override
  {
    aux->modify<Mword>(0x200, 0xfffffdff, reg);
  }
};
using Pfc_omap = Pfc_omap_4;
#endif

#ifdef CONFIG_PF_OMAP5_5432EVM
struct Pfc_omap_5 : Pfc_omap_4_5
{
  [[noreturn]] void system_reboot() override
  {
    enum
    {
      DEVICE_PRM         = Mem_layout::Prm_phys_base + 0x1c00,
      PRM_RSTCTRL        = DEVICE_PRM + 0,
      RST_GLOBAL_COLD_SW = 1 << 1,
    };
    Address p = Kmem_mmio::remap(PRM_RSTCTRL, sizeof(Mword));

    Io::set<Mword>(RST_GLOBAL_COLD_SW, p);
    Io::read<Mword>(p);

    L4::infinite_loop();
  }

  void setup_ap_boot(Mmio_register_block *aux, unsigned reg) override
  {
    aux->write<Mword>(0x20, reg);
  }
};
using Pfc_omap = Pfc_omap_5;

#endif

static Pfc_singleton<Pfc_omap> __pfc;
