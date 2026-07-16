
#include <pfc-arm.h>
#include <pfc-psci.h>
#include <types.h>
#include <ipi.h>
#include <globalconfig.h>
#include <infinite_loop.h>
#include <mmio_register_block.h>
#include <kmem_mmio.h>

#include <cstdio>

struct Pfc_ls_nopsci : Pfc_arm
{
  Mmio_register_block reset;
  Mmio_register_block devcon;

  Pfc_ls_nopsci()
  : reset(Kmem_mmio::map(0x02ad0000, sizeof(Unsigned16))),
    devcon(Kmem_mmio::map(0x01ee0000, 0x1000))
  {}

  void system_reboot() override
  {
    reset.r<16>(0x0) = 1 << 2;
    L4::infinite_loop();
  }

#ifdef CONFIG_MP
  bool do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    enum { DCFG_CCSR_SCRATCHRW1 = 0x200 };
    devcon.r<32>(DCFG_CCSR_SCRATCHRW1) = __builtin_bswap32(phys_tramp_mp_addr);
    Mem::mp_wmb();
    Ipi::bcast(Ipi::Global_request, Cpu_number::boot_cpu());
    return true;
  }
#endif
};


struct Pfc_ls_psci : Pfc_psci
{
  bool do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    boot_ap_cpus_psci(phys_tramp_mp_addr, { 0xf01 });
    return true;
  }
};

#ifdef CONFIG_ARM_PSCI
using Pfc_ls = Pfc_ls_psci;
#else
using Pfc_ls = Pfc_ls_nopsci;
#endif

static Pfc_singleton<Pfc_ls> __pfc;
