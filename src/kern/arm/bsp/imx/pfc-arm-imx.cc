
#include <pfc-arm.h>
#include <pfc-psci.h>
#include <types.h>
#include <ipi.h>
#include <globalconfig.h>
#include <infinite_loop.h>
#include <mmio_register_block.h>
#include <mem_layout.h>
#include <kmem.h>
#include <cstdio>
#include <processor.h>
#include <mem.h>
#include <cpu.h>

namespace {

struct Pfc_imx_wdog_rst : Pfc_arm
{
  virtual void cpus_off() {};
  [[noreturn]] void system_reboot() override
  {
    enum {
        WCR     = Mem_layout::Watchdog_phys_base + 0,
        WCR_WDE = 1 << 2,
    };

    cpus_off();

    // Enable watchdog with smallest timeout possible (0.5s)
    Io::modify<Unsigned16>(WCR_WDE, 0xff10, Kmem::mmio_remap(WCR,
          sizeof(Unsigned16)));

    L4::infinite_loop();
  }
};

struct Pfc_imx6 : Pfc_imx_wdog_rst
{
  enum
  {
    SRC_SCR  = 0,
    SRC_GPR3 = 0x28,
    SRC_GPR5 = 0x30,
    SRC_GPR7 = 0x38,

    SRC_SCR_CORE1_3_ENABLE = 7 << 22,
    SRC_SCR_CORE1_3_RESET  = 7 << 14,
  };

  Register_block<32> src;

  Pfc_imx6() : src(Kmem::mmio_remap(0x020d8000 /*Src_phys*/, 0x100))
  {}

  void do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    src[SRC_GPR3] = phys_tramp_mp_addr;
    src[SRC_GPR5] = phys_tramp_mp_addr;
    src[SRC_GPR7] = phys_tramp_mp_addr;

    src[SRC_SCR].set(SRC_SCR_CORE1_3_ENABLE | SRC_SCR_CORE1_3_RESET);
  }

  void cpus_off() override
  {
    // switch off core1-3
    src[SRC_SCR].clear(SRC_SCR_CORE1_3_ENABLE);
  }
};

using Pfc_imx6ul = Pfc_imx_wdog_rst;

struct Pfc_imx7_nopsci : Pfc_imx_wdog_rst
{
  enum
  {
    GPC_CPU_PGC_SW_PUP_REQ             = 0x0f0,
    GPC_PGC_A7CORE1_CTRL               = 0x840,

    GPC_CPU_PGC_SW_PUP_REQ_CORE1_A7    = 1 << 1,
    GPC_PGC_A7CORE1_CTRL_PCR           = 1 << 0,

    SRC_A7RCR1                         = 0x08,
    SRC_GPR3                           = 0x7C,

    SRC_A7RCR_A7_CORE1_ENABLE          = 1 << 1,
  };

  Register_block<32> src;

  Pfc_imx7_nopsci() : src(Kmem::mmio_remap(0x30390000 /*Src_phys*/, 0x100))
  {}

  void do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {

    Register_block<32> gpc(Kmem::mmio_remap(0x303a0000 /*Gpc_phys*/, 0x1000));

    src[SRC_GPR3] = phys_tramp_mp_addr;

    gpc[GPC_PGC_A7CORE1_CTRL].set(GPC_PGC_A7CORE1_CTRL_PCR); // power off
    gpc[GPC_CPU_PGC_SW_PUP_REQ].set(GPC_CPU_PGC_SW_PUP_REQ_CORE1_A7); // power up 2nd core
    while (gpc[GPC_CPU_PGC_SW_PUP_REQ] & GPC_CPU_PGC_SW_PUP_REQ_CORE1_A7)
      ;
    gpc[GPC_PGC_A7CORE1_CTRL].clear(GPC_PGC_A7CORE1_CTRL_PCR); // enable again

    src[SRC_A7RCR1].set(SRC_A7RCR_A7_CORE1_ENABLE);
  }

  void cpus_off() override
  {
    // switch off core1
    src[SRC_A7RCR1].clear(SRC_A7RCR_A7_CORE1_ENABLE);
  }
};

struct Pfc_imx7_psci : Pfc_psci
{
  void do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    boot_ap_cpus_psci(phys_tramp_mp_addr, { 0x1 });
  }
};

struct Pfc_imx8 : Pfc_psci
{
  void do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    boot_ap_cpus_psci(phys_tramp_mp_addr,
                      { 0x000, 0x001, 0x002, 0x003, 0x100, 0x101 });
  }
};

struct Pfc_imx8mp : Pfc_psci
{
  void do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    boot_ap_cpus_psci(phys_tramp_mp_addr, { 0x000, 0x001, 0x002, 0x003 });
  }
};

struct Pfc_imx95 : Pfc_psci
{
  void do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    boot_ap_cpus_psci(phys_tramp_mp_addr,
                      { 0x000, 0x100, 0x200, 0x300, 0x400, 0x500 }, true);
  }
};

struct Pfc_imx_21 : Pfc_arm
{
  [[noreturn]] void system_reboot() override
  {
    enum {
      WCR  = 0x10002000 /*Watchdog_phys*/ + 0,
      WCR_SRS = 1 << 4, // Software Reset Signal

      PLL_PCCR1        = 0x10027000 /*Pll_phys*/ + 0x24,
      PLL_PCCR1_WDT_EN = 1 << 24,
    };

    // WDT CLock Enable
    Io::set<Unsigned32>(PLL_PCCR1_WDT_EN, Kmem::mmio_remap(PLL_PCCR1,
                                                           sizeof(Unsigned32)));

    // Assert Software reset signal by making the bit zero
    Io::mask<Unsigned16>(~WCR_SRS, Kmem::mmio_remap(WCR, sizeof(Unsigned16)));

    L4::infinite_loop();
  }
};

struct Pfc_imx_28 : Pfc_arm
{
  [[noreturn]] void system_reboot() override
  {
    Register_block<32> r(Kmem::mmio_remap(0x80056000, 0x100));
    r[0x50] = 1; // Watchdog counter
    r[0x04] = 1 << 4; // Watchdog enable

    L4::infinite_loop();
  }
};

#ifdef CONFIG_PF_IMX_21
using Pfc_imx = Pfc_imx_21;
#endif
#ifdef CONFIG_PF_IMX_28
using Pfc_imx = Pfc_imx_28;
#endif
#ifdef CONFIG_PF_IMX_35
using Pfc_imx = Pfc_imx_wdog_rst;
#endif
#ifdef CONFIG_PF_IMX_51
using Pfc_imx = Pfc_imx_wdog_rst;
#endif
#ifdef CONFIG_PF_IMX_53
using Pfc_imx = Pfc_imx_wdog_rst;
#endif
#ifdef CONFIG_PF_IMX_6UL
using Pfc_imx = Pfc_imx_wdog_rst;
#endif
#ifdef CONFIG_PF_IMX_6
using Pfc_imx = Pfc_imx6;
#endif
#ifdef CONFIG_PF_IMX_7
#ifdef CONFIG_ARM_PSCI
using Pfc_imx = Pfc_imx7_psci;
#else
using Pfc_imx = Pfc_imx7_nopsci;
#endif
#endif
#ifdef CONFIG_PF_IMX_8M
using Pfc_imx = Pfc_imx8;
#endif
#ifdef CONFIG_PF_IMX_8MP
using Pfc_imx = Pfc_imx8mp;
#endif
#ifdef CONFIG_PF_IMX_8XQ
using Pfc_imx = Pfc_imx8;
#endif
#ifdef CONFIG_PF_IMX_95
using Pfc_imx = Pfc_imx95;
#endif

static Pfc_singleton<Pfc_imx> __pfc;
}
