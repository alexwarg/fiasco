#include <pfc.h>
#include <pfc-acpi.h>
#include <pfc-ia32.h>
#include <reset.h>

struct Pfc_pc : Pfc_acpi<Pfc_ia32>
{
  [[noreturn]] void system_off() override
  { this->system_reboot(); while (1) ;}

  [[noreturn]] void system_reboot() override
  { platform_reset(); }
};

static Pfc_singleton<Pfc_pc> __pfc_pc;
