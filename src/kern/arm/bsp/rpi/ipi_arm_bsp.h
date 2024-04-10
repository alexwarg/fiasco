#pragma once

#include <globalconfig.h>
#include <std_macros.h>

#include <arm_control.h>
#include <cpu.h>

struct Ipi_arch_base
{
  enum Message { Global_request, Request, Debug, Timer };

  static Message pending()
  {
    return (Message)Arm_control::o()->ipi_pending();
  }

protected:
  Cpu_phys_id _phys_id;
};

template<typename IPI>
struct Ipi_arch : Ipi_arch_base
{
  static void init(Cpu_number cpu)
  {
    IPI::_ipi.cpu(cpu)._phys_id = Proc::cpu_id();
    Arm_control::o()->mailbox_unmask(0, Proc::cpu_id());
  }

  static void send(Message m, Cpu_number from_cpu, Cpu_number to_cpu)
  {
    Arm_control::o()->send_ipi(m, IPI::_ipi.cpu(to_cpu)._phys_id);
    IPI::stat_sent(from_cpu);
  }

  static void send(Message m, Cpu_number from_cpu, Cpu_phys_id to_cpu)
  {
    Arm_control::o()->send_ipi(m, to_cpu);
    IPI::stat_sent(from_cpu);
  }

  static void bcast(Message m, Cpu_number from_cpu)
  {
    for (Cpu_number n = Cpu_number::first(); n < Config::max_num_cpus(); ++n)
      if (Cpu::online(n) && n != from_cpu)
        send(m, from_cpu, n);
  }

  static void eoi(Message, Cpu_number on_cpu)
  {
    IPI::stat_received(on_cpu);
  }
};
