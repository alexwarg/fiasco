#include <pfc-arm.h>
#include <globalconfig.h>

#ifdef CONFIG_MP

#include <tramp-mp.h>

void
Pfc_arm::boot_ap_cpus()
{
  do_boot_ap_cpus(tramp_mp_prepare());
}

#else // CONFIG_MP

void
Pfc_arm::boot_ap_cpus()
{}

#endif // CONFIG_MP
