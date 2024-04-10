#pragma once

#include <pic.h>

#include <globalconfig.h>
#include <std_macros.h>

#ifndef CONFIG_ARM_GIC

#include <ipi_arm_bsp.h>

#else // CONFIG_ARM_GIC

struct Ipi_arch_base
{
  static constexpr unsigned Ipi_start = IS_ENABLED(CONFIG_ARM_EM_TZ) ? 8 : 1;

  enum Message
  {
    Global_request = Ipi_start, Request, Debug, Timer,
    Ipi_end
  };

};

template<typename IPI>
struct Ipi_arch : Ipi_arch_base
{
  static void eoi(Message, Cpu_number on_cpu)
  {
    // with the ARM-GIC we have to do the EOI right after the ACK
    IPI::stat_received(on_cpu);
  }

  static void send(Message m, Cpu_number from_cpu, Cpu_number to_cpu)
  {
    get_gic(from_cpu)->softint_cpu(to_cpu, m);
    IPI::stat_sent(from_cpu);
  }

  static void bcast(Message m, Cpu_number from_cpu)
  {
    get_gic(from_cpu)->softint_bcast(m);
  }

  static void init(Cpu_number)
  {}

private:
// HACK for per CPU GIC
#ifdef CONFIG_PF_EXYNOS_EXTGIC
  static decltype(Pic::gic.cpu(Cpu_number()))
  get_gic(Cpu_number c)
  { return Pic::gic.cpu(c); }
#else
  static decltype(Pic::gic)
  get_gic(Cpu_number)
  { return Pic::gic; }
#endif
};

#endif // CONFIG_ARM_GIC
