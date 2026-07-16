
#include <pfc-dt.h>
#include <device_tree.h>
#include <types.h>
#include <processor.h>
#include <mem.h>
#include <cpu.h>
#include <mmio_register_block.h>
#include <ipi.h>
#include <io.h>
#include <kmem_mmio.h>
#include <mem_layout.h>
#include <infinite_loop.h>

#if defined (CONFIG_BIT32) && !defined (CONFIG_PF_RPI_RPI1) && !defined(CONFIG_PF_RPI_PRIZW)
#include <arm_control.h>
#endif

namespace {

#ifndef CONFIG_PF_RPI_RPI5

struct Pfc_z : Pfc_dt
{
  static void bcm2711_reset(Address base)
  {
    enum { Rstc = 0x1c, Wdog = 0x24 };
    Mword pw = 0x5a << 24;
    Io::write<Unsigned32>(pw | 8, base + Wdog);
    Io::write<Unsigned32>((Io::read<Unsigned32>(base + Rstc) & ~0x30)
        | pw | 0x20,
        base + Rstc);
  }

  [[noreturn]] void system_reboot() override
  {
    if (Device_tree::dt.valid())
      {
#ifdef CONFIG_DT
        Device_tree::Node n = Device_tree::dt.node_by_compatible("brcm,bcm2711-pm");
        if (n.is_valid())
          {
            uint64_t phys;
            if (n.get_reg(0, &phys))
              bcm2711_reset(Kmem_mmio::remap(phys, 0x100));
          }
#endif
      }

    bcm2711_reset(Kmem_mmio::remap(Mem_layout::Watchdog_phys_base, 0x100));
    L4::infinite_loop();
  }

#if !defined (CONFIG_PF_RPI_RPI1) && !defined (CONFIG_PF_RPI_PRIZW)
#ifdef CONFIG_BIT64
  bool do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    if (Pfc_dt::do_boot_ap_cpus(phys_tramp_mp_addr))
      return true;

    Mmio_register_block a(Kmem_mmio::map(0xd8, 0x28));
    Cpu_phys_id myid = Proc::cpu_id();
    int seq = 1;
    for (unsigned i = 0; i < min<unsigned>(4, Config::Max_num_cpus); ++i)
      if (myid != Cpu_phys_id(i))
        {
          a.r<64>(i * 8) = phys_tramp_mp_addr;
          Mem_unit::clean_dcache();
          asm volatile("sev");

          // All at once does not work, so wait one-by-one
          while (!Cpu::online(Cpu_number(seq)))
            {
              Mem::barrier();
              Proc::pause();
            }
          ++seq;
        }
    return true;
  }
#else
#ifdef CONFIG_MP
  bool do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    Cpu_phys_id myid = Proc::cpu_id();
    int seq = 1;
    for (unsigned i = 0; i < min<unsigned>(4, Config::Max_num_cpus); ++i)
      if (myid != Cpu_phys_id(i))
        {
          Arm_control::o()->do_boot_cpu(Cpu_phys_id(i), phys_tramp_mp_addr);
          while (!Cpu::online(Cpu_number(seq)))
            {
              Mem::barrier();
              Proc::pause();
            }
          ++seq;
        }

    return true;
  }
#endif
#endif
#endif
};

static Pfc_singleton<Pfc_z> __pfc;

#else // CONFIG_PF_RPI_RPI5


struct Pfc_rpi5 : Pfc_dt
{
  bool do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    if (Pfc_dt::do_boot_ap_cpus(phys_tramp_mp_addr))
      return true;

    boot_ap_cpus_psci(phys_tramp_mp_addr, { 0x000, 0x100, 0x200, 0x300 }, true);
    return true;
  }
};

static Pfc_singleton<Pfc_rpi5> __pfc;

#endif // CONFIG_PF_RPI_RPI5

}
