#pragma once

#include <pfc.h>
#include <infinite_loop.h>

class Pfc_arm : public virtual Pfc
{
public:
  virtual bool do_boot_ap_cpus(Address entry_phys)
  {
    (void) entry_phys;
    return false;
  }

  void boot_ap_cpus() override;

  [[noreturn]] void system_off() override
  {
    system_reboot();
    L4::infinite_loop();
  }
};
