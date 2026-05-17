
#include <pfc-arm.h>
#include <pfc-psci.h>
#include <types.h>
#include <processor.h>
#include <mem.h>
#include <cpu.h>
#include <mmio_register_block.h>
#include <ipi.h>
#include <kmem.h>
#include <infinite_loop.h>
#include <minmax.h>
#include <psci.h>
#include <cpu.h>
#include <mem.h>
#include <globalconfig.h>
#include <koptions.h>

#include <cstdio>

namespace {

#ifdef CONFIG_ARM_PSCI

#if defined (CONFIG_PF_QCOM_MSM8909) || defined (CONFIG_PF_QCOM_MSM8916)
  static unsigned const psci_coreid[] =
    { 0x0, 0x1, 0x2, 0x3 };
#endif

#if defined (CONFIG_PF_QCOM_MSM8939)
  static unsigned const psci_coreid[] =
    { 0x000, 0x001, 0x002, 0x003, 0x100, 0x101, 0x102, 0x103 };
#endif

#if defined (CONFIG_PF_QCOM_SM8150)
  static unsigned const psci_coreid[] =
    { 0x000, 0x100, 0x200, 0x300, 0x400, 0x500, 0x600, 0x700 };
#endif

struct Pfc_qc_psci : Pfc_psci
{
  void do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    unsigned ncpus = min<unsigned>(cxx::size(psci_coreid), Config::Max_num_cpus);
    for (unsigned i = 0; i < ncpus; ++i)
      {
        int r = Psci::cpu_on(psci_coreid[i], phys_tramp_mp_addr);
        if (r && r != Psci::Psci_already_on)
          printf("CPU%d boot-up error: %d\n", i, r);
      }
  }
};

using Pfc_qc = Pfc_qc_psci;

#else // CONFIG_ARM_PSCI

struct Pfc_qc_nopsci : Pfc_arm
{
  [[noreturn]] void system_reboot() override
  {
    Address base = Kmem::mmio_remap(Mem_layout::Mpm_ps_hold, sizeof(Unsigned32));
    Io::write<Unsigned32>(0, base);
    L4::infinite_loop();
  }

  void do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    if (Koptions::o()->core_spin_addr == -1ULL)
      return;

    Address base = Kmem::mmio_remap(Koptions::o()->core_spin_addr, sizeof(Address));
    Io::write<Address>(phys_tramp_mp_addr, base);
    Mem::dsb();
    asm volatile("sev" : : : "memory");
  }
};

using Pfc_qc = Pfc_qc_nopsci;
#endif // CONFIG_ARM_PSCI

static Pfc_singleton<Pfc_qc> __pfc;

}
