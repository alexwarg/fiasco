
#include <pfc-arm.h>
#include <types.h>
#include <ipi.h>
#include <globalconfig.h>
#include <infinite_loop.h>
#include <mmio_register_block.h>
#include <mem_layout.h>
#include <kmem_mmio.h>
#include <poll_timeout_kclock.h>
#include <io.h>

#include <cstdio>

namespace {

struct Pfc_base : Pfc_arm
{
  enum
  {
    Reset_vector_addr              = 0x6000f100,
  };

  Pfc_base()
  : pmc(Kmem_mmio::map(Mem_layout::Pmc_phys_base, 0x100))
  {}

  [[noreturn]] void system_reboot() override
  {
    pmc.modify<Mword>(0x10, 0, 0);
    L4::infinite_loop();
  }

protected:
  Mmio_register_block pmc;
};

struct Pfc_tegra3 : Pfc_base
{
  enum
  {
    PMC_PWRGATE_TOGGLE          = 0x30,
    PMC_PWRGATE_REMOVE_CLAMPING = 0x34,
    PMC_PWRGATE_STATUS_0        = 0x38,

    PMC_PWRGATE_TOGGLE_START = 1 << 8,

    CLK_RST_CONTROLLER_RST_CPU_CMPLX_SET   = 0x340,
    CLK_RST_CONTROLLER_RST_CPU_CMPLX_CLR   = 0x344,
    CLK_RST_CONTROLLER_CLK_CPU_CMPLX_SET_0 = 0x348,
    CLK_RST_CONTROLLER_CLK_CPU_CMPLX_CLR_0 = 0x34c,
  };

  void do_boot_ap_cpus(Address phys_reset_vector) override
  {
    // set (temporary) new reset vector
    Io::write<Mword>(phys_reset_vector, Kmem_mmio::remap(Reset_vector_addr,
          sizeof(Mword)));

    int cpu_powergates[4]        = { 0, 9, 10, 11 };
    int flowctrl_cpu_halt_ofs[4] = { 0, 0x14, 0x1c, 0x24 };
    int flowctrl_cpu_csr_ofs[4]  = { 8, 0x18, 0x20, 0x28 };
    Mmio_register_block clk_rst(Kmem_mmio::map(Mem_layout::Clock_reset_phys_base,
                                                 0x1000));
    Mmio_register_block flow_ctrl(Kmem_mmio::map(0x60007000, 0x100));

    for (unsigned i = 1; i < 4; ++i)
      {
        assert(i < 4);
        int gate = cpu_powergates[i];

        // put cpu into reset
        clk_rst.write<Mword>(0x1111 << i, CLK_RST_CONTROLLER_RST_CPU_CMPLX_SET);
        Mem::dmb();

        // flowctrl halt
        flow_ctrl.write<Mword>(0, flowctrl_cpu_halt_ofs[i]);
        flow_ctrl.read<Mword>(flowctrl_cpu_halt_ofs[i]);

        if (!pwr_status(gate))
          {
            pmc.write<Mword>(PMC_PWRGATE_TOGGLE_START | gate, PMC_PWRGATE_TOGGLE);

            Poll_timeout_kclock pt(100000);
            while (pt.test(pwr_status(gate)))
              Proc::pause();

            if (pt.timed_out())
              return;
          }

        // power is on now

        // enable cpu clock
        clk_rst.write<Mword>(1 << (8 + i), CLK_RST_CONTROLLER_CLK_CPU_CMPLX_CLR_0);
        clk_rst.read<Mword>(CLK_RST_CONTROLLER_CLK_CPU_CMPLX_CLR_0);
        delay();

        // remove clamping
        pmc.write<Mword>(1 << gate, PMC_PWRGATE_REMOVE_CLAMPING);
        delay();

        // clear flow csr
        flow_ctrl.write<Mword>(0, flowctrl_cpu_csr_ofs[i]);
        Mem::wmb();
        flow_ctrl.read<Mword>(flowctrl_cpu_csr_ofs[i]);

        // out of reset
        clk_rst.write<Mword>(0x1111 << i, CLK_RST_CONTROLLER_RST_CPU_CMPLX_CLR);
        Mem::wmb();
      }
  }

private:
  static void delay()
  {
    for (int delay = 20000; delay; --delay)
      Mem::barrier();
  }

  Mword pwr_status(int gate)
  {
    return pmc.read<Mword>(PMC_PWRGATE_STATUS_0) & (1 << gate);
  }
};


struct Pfc_tegra2 : Pfc_base
{
  enum
  {
    Cpu_complex                    = 0x60006000,
    Clk_rst_ctrl     = 0x004c,
    Clk_rst_ctrl_clr = 0x0344,
    Unhalt_addr      = 0x1014,

  };

  void do_boot_ap_cpus(Address phys_reset_vector) override
  {
    // set (temporary) new reset vector
    Io::write<Mword>(phys_reset_vector, Kmem_mmio::remap(Reset_vector_addr,
          sizeof(Mword)));

    Mmio_register_block cpu_complex(Kmem_mmio::map(Cpu_complex, 0x2000));

    // clocks on other cpu
    Mword r = cpu_complex.read<Mword>(Clk_rst_ctrl);
    cpu_complex.write<Mword>(r & ~(1 << 9), Clk_rst_ctrl);
    cpu_complex.write<Mword>((1 << 13) | (1 << 9) | (1 << 5) | (1 << 1), Clk_rst_ctrl_clr);
    // kick cpu1
    cpu_complex.write<Mword>(0, Unhalt_addr);
  }
};

#ifdef CONFIG_PF_TEGRA2
using Pfc_tegra = Pfc_tegra2;
#endif
#ifdef CONFIG_PF_TEGRA3
using Pfc_tegra = Pfc_tegra3;
#endif

static Pfc_singleton<Pfc_tegra> __pfc;

}
