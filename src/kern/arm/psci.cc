
#include <psci.h>
#include <cstdio>

void
Psci::init()
{
  printf("Detecting PSCI ...\n");
  Result r = psci_call(Psci_version);
  printf("Detected PSCI v%ld.%ld\n", r.res[0] >> 16, r.res[0] & 0xffff);

  bool is_v1 = (r.res[0] >> 16) >= 1;

  if (is_v1)
    {
      r = psci_call(Psci_features, psci_fn(Psci_cpu_suspend));
      if (r.res[0] & (1UL << 31))
        printf("PSCI: CPU_Suspend not supported (%d)\n", (int)r.res[0]);
      else
        printf("PSCI: CPU_SUSPEND format %s, %s OS-initiated mode\n",
               r.res[0] & 2 ? "extended" : "original v0.2",
               r.res[0] & 1 ? "supports" : "does not support");
    }

  r = psci_call(Psci_migrate_info_type);

  if (r.res[0] == 0 || r.res[0] == 1)
    printf("PSCI: TOS: single core, %smigration capable.\n",
           r.res[0] ? "not " : "");
  else
    printf("PSCI: TOS: Not present or not required.\n");
}

int
Psci::cpu_on(unsigned long target, Address phys_tramp_mp_addr)
{
  Result r = psci_call(Psci_cpu_on, target, phys_tramp_mp_addr);
  return r.res[0];
}

void
Psci::system_reset()
{
  psci_call(Psci_system_reset);
  printf("PSCI reset failed.\n");
}

void
Psci::system_off()
{
  psci_call(Psci_system_off);
  printf("PSCI system-off failed.\n");
}
