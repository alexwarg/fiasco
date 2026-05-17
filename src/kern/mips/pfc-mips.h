#pragma once

#include <pfc.h>
#include <infinite_loop.h>

class Pfc_mips : public virtual Pfc
{
public:
  void boot_ap_cpus() override;

  [[noreturn]] void system_off() override
  {
    system_reboot();
    L4::infinite_loop();
  }

};
