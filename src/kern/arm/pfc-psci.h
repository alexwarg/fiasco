#pragma once

#include <pfc-arm.h>
#include <psci.h>
#include <infinite_loop.h>
#include <system_clock.h>
#include <cpu.h>
#include <mem.h>

#include <cstdio>

struct Pfc_psci : Pfc_arm
{
  Pfc_psci()
  {
    Psci::init();
  }

  int cpu_on(unsigned long target, Address phys_tramp_mp_addr)
  {
    return Psci::cpu_on(target, phys_tramp_mp_addr);
  }

  template<size_t NUM>
  static void
  boot_ap_cpus_psci(Address phys_tramp_mp_addr,
                    const unsigned long (&physid_list)[NUM],
                    bool /* sequential */ = false)
  {
    /* The current implementation is booting cores sequentially only */

    unsigned seq = 1;
    for (unsigned long physid : physid_list)
      {
        if (seq >= Config::Max_num_cpus)
          break;

        int r = Psci::cpu_on(physid, phys_tramp_mp_addr);
        if (r)
          {
            if (r != Psci::Psci_already_on)
              printf("CPU%d/%lx boot-up error: %d\n", seq, physid, r);
            continue;
          }

        Unsigned64 timeout = System_clock::clock() + 500 * 1000;
        while (1)
          {
            if (Cpu::online(Cpu_number(seq)))
              break;

            if (System_clock::clock() > timeout)
              {
                printf("CPU%d/%lx did not come online.\n", seq, physid);
                break;
              }

            Mem::barrier();
            Proc::pause();
          }
        ++seq;
      }
  }

  [[noreturn]] void system_reboot() override
  {
    Psci::system_reset();
    L4::infinite_loop();
  }

  [[noreturn]] void system_off() override
  {
    Psci::system_off();
    L4::infinite_loop();
  }
};
