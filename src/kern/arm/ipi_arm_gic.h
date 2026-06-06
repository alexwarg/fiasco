#pragma once

#include <globalconfig.h>
#include <std_macros.h>
#include <gic_iface.h>

struct Ipi_arm_gic_base
{
  static constexpr unsigned Ipi_start = IS_ENABLED(CONFIG_ARM_EM_TZ) ? 8 : 1;

  enum Message
  {
    Global_request = Ipi_start, Request, Debug, Timer,
    Ipi_end
  };

  static void init(Cpu_number)
  {}
};

template<typename IPI>
struct Ipi_arm_gic : Ipi_arm_gic_base
{
  static void eoi(Message, Cpu_number on_cpu)
  {
    // with the ARM-GIC we have to do the EOI right after the ACK
    IPI::stat_received(on_cpu);
  }

  static void send(Message m, Cpu_number from_cpu, Cpu_number to_cpu)
  {
    Gic::primary->softint_cpu(to_cpu, m);
    IPI::stat_sent(from_cpu);
  }

  static void bcast(Message m, Cpu_number /*from_cpu*/)
  {
    Gic::primary->softint_bcast(m);
  }
};

