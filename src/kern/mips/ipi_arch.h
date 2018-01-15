#pragma once


#include <globalconfig.h>
#include <std_macros.h>

#include <ipi_control.h>
#include <processor.h>
#include <types.h>
#include <cpu_mask.h>
#include <cpu.h>

template<typename IPI>
struct Ipi_arch
{
  enum Message { Request, Global_request, Debug, Num_requests };

  static Ipi_control *hw;

  Mword atomic_reset(Message m)
  {
    Mword tmp;
    asm volatile (
        ".set push; .set noreorder; .set noat \n"
        "1: move  $at, $zero       \n"
        "   ll    %[tmp], %[ptr]   \n"
        "   sc    $at, %[ptr]      \n"
        "   beqz  $at, 1b          \n"
        "     nop                  \n"
        "   sync                   \n"
        ".set pop                  \n"
        : [tmp] "=&r" (tmp), [ptr] "+m"(_rq[m]));
    return tmp;
  }

  static IPI *ipis(Cpu_number c)
  { return &IPI::_ipi.cpu(c); }

  static Mword atomic_reset(Cpu_number cpu, Message m)
  { return IPI::_ipi.cpu(cpu).atomic_reset(m); }

  static void init(Cpu_number cpu)
  {
    auto &ipi = IPI::_ipi.cpu(cpu);
    for (auto &r: ipi._rq)
      r = 0;

    ipi._phys_id = Proc::cpu_id();
  }

  static void send(Message m, Cpu_number from_cpu, Cpu_number to_cpu)
  {
    auto &ipi = IPI::_ipi.cpu(to_cpu);
    if (access_once(&ipi._rq[m]))
      return;

    write_now(&ipi._rq[m], true);
    asm volatile ("sync" : : : "memory");

    if (access_once(&ipi._rq[m]))
      hw->send_ipi(to_cpu, &ipi);

    IPI::stat_sent(from_cpu);
  }

  static void eoi(Message, Cpu_number on_cpu)
  {
    IPI::stat_received(on_cpu);
  }

  static void bcast(Message m, Cpu_number from_cpu)
  {
    (void)from_cpu;
    Cpu_mask ipis;
    Cpu_number max = Cpu_number::first();
    for (Cpu_number n: Cpu::all_online())
      {
        auto &ipi = IPI::_ipi.cpu(n);
        if (access_once(&ipi._rq[m]))
          continue;

        write_now(&ipi._rq[m], true);
        ipis.set(n);
        max = n;
      }

    asm volatile ("sync" : : : "memory");

    for (Cpu_number n = Cpu_number::first(); n < max; ++n)
      {
        if (!ipis.get(n))
          continue;

        auto &ipi = IPI::_ipi.cpu(n);
        if (access_once(&ipi._rq[m]))
          hw->send_ipi(n, &ipi);
      }
  }

private:
  Mword _rq[Num_requests];
  Cpu_phys_id _phys_id;
};

template<typename IPI>
Ipi_control *Ipi_arch<IPI>::hw;
