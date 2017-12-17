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

  Apic_id boot_apic_id = Apic::get_id();

  // make sure the boot CPU gets the right CPU number
  kernel_cpu_id_map.set(Cpu_number::boot_cpu(), boot_apic_id);

  Cpu_number last_cpu = Cpu_number::first();

  // xAPIC: Collect all *enabled* CPUs and assign them the leading CPU numbers.
  for (unsigned entry = 0; last_cpu < Config::max_num_cpus(); ++entry)
    {
      auto const *lapic = madt->find<Acpi_madt::Lapic>(entry);
      if (!lapic)
        break;

      if (!(lapic->flags & 1))
        continue; // skip disabled entries

      Apic_id aid = Apic::acpi_lapic_to_apic_id(lapic->apic_id);

      if (aid == boot_apic_id)
        continue; // boot CPU already has a CPU number assigned

      // skip logical boot CPU number
      if (last_cpu == Cpu_number::boot_cpu())
        ++last_cpu; // skip logical boot CPU number

      kernel_cpu_id_map.set(last_cpu++, aid);
    }

  // x2APIC: According to ACPI 5.2.12.12, logical processors with APIC ID values
  // less than 255 must use the Processor Local APIC structure but there is
  // hardware which has only MADT entry type LOCAL_X2AIC but no MADT entry type
  // LAPIC!
  for (unsigned entry = 0; last_cpu < Config::max_num_cpus(); ++entry)
    {
      auto const *lx2apic = madt->find<Acpi_madt::Local_x2apic>(entry++);
      if (!lx2apic)
        break;

      if (!(lx2apic->flags & 1))
        continue; // skip disabled entries

      Apic_id aid{lx2apic->apic_id};

      if (aid == boot_apic_id)
        continue; // boot CPU already has a CPU number assigned

      // skip logical boot CPU number
      if (last_cpu == Cpu_number::boot_cpu())
        ++last_cpu; // skip logical boot CPU number

      kernel_cpu_id_map.set(last_cpu++, aid);
    }

  // Collect all *disabled* CPUs and assign them the remaining CPU numbers
  // to make sure that we can boot at least the maximum number of enabled CPUs.
  // Disabled CPUs may come online later by hot plugging.
  for (unsigned entry = 0; last_cpu < Config::max_num_cpus(); ++entry)
    {
      auto const *lapic = madt->find<Acpi_madt::Lapic>(entry);
      if (!lapic)
        break;

      if (lapic->flags & 1)
        continue; // skip enabled entries

      if (last_cpu == Cpu_number::boot_cpu())
        ++last_cpu; // skip logical boot CPU number
                    //
      Apic_id aid = Apic::acpi_lapic_to_apic_id(lapic->apic_id);
      kernel_cpu_id_map.set(last_cpu++, aid);
    }

  for (unsigned entry = 0; last_cpu < Config::max_num_cpus(); ++entry)
    {
      auto const *lx2apic = madt->find<Acpi_madt::Local_x2apic>(entry);
      if (!lx2apic)
        break;

      if (lx2apic->flags & 1)
        continue; // skip enabled entries

      Apic_id aid{lx2apic->apic_id};

      if (aid != Apic_id{0xffffffff}) // ignore dummy entries
        kernel_cpu_id_map.set(last_cpu++, aid);
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
