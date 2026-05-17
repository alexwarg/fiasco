#pragma once

#include <pfc-arm.h>
#include <psci.h>
#include <infinite_loop.h>

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
