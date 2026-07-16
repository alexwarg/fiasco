
#include <pfc-dt.h>
#include <device_tree.h>
#include <config.h>
#include <cpu.h>
#include <mem.h>
#include <processor.h>
#include <warn.h>
#include <globalconfig.h>

#ifdef CONFIG_DT
#include <kmem_mmio.h>
#include <mem_unit.h>
#include <system_clock.h>

#include <cstdio>

static bool
wait_for_cpu_online(unsigned seq, unsigned long mpidr)
{
  Unsigned64 timeout = System_clock::clock() + 500 * 1000;
  while (1)
    {
      if (Cpu::online(Cpu_number(seq)))
        return true;

      if (System_clock::clock() > timeout)
        {
          printf("CPU%u/%lx did not come online.\n", seq, mpidr);
          return false;
        }

      Mem::barrier();
      Proc::pause();
    }
}

static bool
boot_cpu_psci(unsigned seq, unsigned long mpidr, Address phys_tramp)
{
#ifdef CONFIG_ARM_PSCI
  int r = Psci::cpu_on(mpidr, phys_tramp);
  if (r)
    {
      if (r != Psci::Psci_already_on)
        printf("CPU%u/%lx boot-up error: %d\n", seq, mpidr, r);
      return false;
    }
  return wait_for_cpu_online(seq, mpidr);
#else
  printf("ERROR: PSCI boot not supported\n");
  return false;
#endif
}

static bool
boot_cpu_spin_table(unsigned seq, unsigned long mpidr,
                    uint64_t release_addr, Address phys_tramp)
{
  void *va = Kmem_mmio::map(release_addr, sizeof(uint64_t), true);
  if (!va)
    {
      WARNX(Error, "CPU%u/%lx: cannot map cpu-release-addr %llx\n",
             seq, mpidr, (unsigned long long)release_addr);
      return false;
    }

  *static_cast<volatile uint64_t *>(va) = phys_tramp;
  Mem_unit::clean_dcache(va, static_cast<char *>(va) + sizeof(uint64_t));
  asm volatile("sev");

  return wait_for_cpu_online(seq, mpidr);
}
#endif // CONFIG_DT

bool
Pfc_dt::do_boot_ap_cpus(Address phys_tramp_mp_addr [[maybe_unused]])
{
  if (!Device_tree::dt.valid())
    return false;

#ifdef CONFIG_DT
  Device_tree::Node cpus = Device_tree::dt.node_by_path("/cpus");
  if (!cpus.is_valid())
    return false;

  char const *default_method = cpus.get_prop_str("enable-method");

  unsigned long boot_mpidr = Cpu::mpidr() & 0xff00ffffffUL;
  unsigned seq = 1;

  cpus.for_each_subnode([&](Device_tree::Node cpu)
    {
      if (seq >= Config::Max_num_cpus)
        return Device_tree::Break;

      if (!cpu.check_device_type("cpu"))
        return Device_tree::Continue;

      if (!cpu.is_enabled())
        return Device_tree::Continue;

      uint64_t reg;
      if (!cpu.get_reg_untranslated(0, &reg))
        return Device_tree::Continue;

      if (reg == boot_mpidr)
        return Device_tree::Continue;

      char const *method = cpu.get_prop_str("enable-method");
      if (!method)
        method = default_method;

      if (!method)
        {
          WARNX(Warning, "CPU '%s': no enable-method\n",
                cpu.get_name("?"));
          return Device_tree::Continue;
        }

      if (__builtin_strcmp(method, "psci") == 0)
        {
          if (boot_cpu_psci(seq, reg, phys_tramp_mp_addr))
            ++seq;
        }
      else if (__builtin_strcmp(method, "spin-table") == 0)
        {
          uint64_t release_addr;
          if (!cpu.get_prop_u64("cpu-release-addr", &release_addr))
            {
              WARNX(Warning, "CPU '%s': no cpu-release-addr\n",
                    cpu.get_name("?"));
              return Device_tree::Continue;
            }
          if (boot_cpu_spin_table(seq, reg, release_addr, phys_tramp_mp_addr))
            ++seq;
        }
      else
        {
          WARNX(Warning, "CPU '%s': unsupported enable-method: %s\n",
                cpu.get_name("?"), method);
        }

      return Device_tree::Continue;
    });
  return true;
#endif // CONFIG_DT
}
