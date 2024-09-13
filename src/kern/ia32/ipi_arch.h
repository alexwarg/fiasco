#pragma once

#include <apic.h>

#include <globalconfig.h>
#include <std_macros.h>

class Ipi_arch_base
{
protected:
  Apic_id _apic_id = ~0;

public:
  enum Message
  {
    Request        = APIC_IRQ_BASE - 1,
    Global_request = APIC_IRQ_BASE + 2,
    Debug          = APIC_IRQ_BASE - 2
  };
};

template<typename IPI>
struct Ipi_arch : Ipi_arch_base
{
  /**
   * \param cpu the logical CPU number of the current CPU.
   * \pre cpu == current CPU.
   */
  static void init(Cpu_number cpu)
  {
    IPI::_ipi.cpu(cpu)._apic_id = Apic::get_id();
  }

  static void eoi(Message, Cpu_number cpu)
  {
    Apic::mp_ipi_ack();
    IPI::stat_received(cpu);
  }

  static void send(Message m, Cpu_number from_cpu, Cpu_number to_cpu)
  {
    Apic::mp_send_ipi(IPI::_ipi.cpu(to_cpu)._apic_id, (Unsigned8)m);
    IPI::stat_sent(from_cpu);
  }

  static void bcast(Message m, Cpu_number from_cpu)
  {
    (void)from_cpu;
    Apic::mp_send_ipi(Apic::APIC_IPI_OTHERS, (Unsigned8)m);
  }
};


