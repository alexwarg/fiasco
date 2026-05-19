#include <kernel_thread.h>

#include <apic.h>
#include <config.h>
#include <cpu.h>
#include <io_apic.h>
#include <irq_mgr.h>
#include <koptions.h>
#include <mem_layout.h>
#include <pfc.h>
#include <trap_state.h>
#include <watchdog.h>

#include <globalconfig.h>

#if 0
IMPLEMENT inline NEEDS["mem_layout.h"]
void
Kernel_thread::free_initcall_section()
{
  // just fill up with invalid opcodes
  for (char *i = const_cast<char *>(Mem_layout::initcall_start);
       i + 1 < const_cast<char *>(Mem_layout::initcall_end); i += 2)
    {
      // UD2
      i[0] = 0x0f;
      i[1] = 0x0b;
    }
}
#endif


#ifdef CONFIG_MP

#include <acpi.h>
#include <cpu_id_map.h>

Cpu_id_map kernel_cpu_id_map;

static void populate_cpu_id_map()
{
  Acpi_madt const *madt = Io_apic::madt();

  // if we cannot find a MADT we cannot boot in deterministic order
  if (!madt)
    return;

  Unsigned32 boot_apic_id = Apic::get_id();

  // make sure the boot CPU gets the right CPU number
  kernel_cpu_id_map.set(Cpu_number::boot_cpu(), boot_apic_id);

  unsigned entry = 0;
  Cpu_number last_cpu = Cpu_number::first();

  // First we collect all enabled CPUs and assign them the leading CPU
  // numbers. Disabled CPUs are collected in a second run and get the
  // remaining CPU numbers assigned. This way we make sure that we can boot
  // at least the maximum number of enabled CPUs. Disabled CPUs may come
  // online later through e.g. hot plugging.
  while (last_cpu < Config::max_num_cpus())
    {
      auto const *lapic = madt->find<Acpi_madt::Lapic>(entry++);
      if (!lapic)
        break;

      // skip disabled CPUs
      if (!(lapic->flags & 1))
        continue;

      // skip logical boot CPU number
      if (last_cpu == Cpu_number::boot_cpu())
        ++last_cpu;

      Unsigned32 aid = ((Unsigned32)lapic->apic_id) << 24;

      // the boot CPU already has a CPU number assigned
      if (aid == boot_apic_id)
        continue;

      kernel_cpu_id_map.set(last_cpu++, aid);
    }

  entry = 0;
  while (last_cpu < Config::max_num_cpus())
    {
      auto const *lapic = madt->find<Acpi_madt::Lapic>(entry++);
      if (!lapic)
        break;

      // skip enabled CPUs
      if (lapic->flags & 1)
        continue;

      // skip logical boot CPU number
      if (last_cpu == Cpu_number::boot_cpu())
        ++last_cpu;

      kernel_cpu_id_map.set(last_cpu++, ((Unsigned32)lapic->apic_id) << 24);
    }
}

#else
inline void populate_cpu_id_map()
{}
#endif

FIASCO_INIT
void
Kernel_thread::bootstrap_arch()
{
  // 
  // install our slow trap handler
  //
  nested_trap_handler      = Trap_state::base_handler;

  extern FIASCO_FASTCALL
  int thread_handle_trap(Trap_state *ts, Cpu_number) asm ("thread_handle_trap");

  Trap_state::base_handler = thread_handle_trap;

  // initialize the profiling timer
  bool user_irq0 = Koptions::o()->opt(Koptions::F_irq0);

  if ((int)Config::Scheduler_mode == Config::SCHED_PIT && user_irq0)
    panic("option -irq0 not possible since irq 0 is used for scheduling");

  populate_cpu_id_map();
  Pfc::get()->boot_ap_cpus();
}
