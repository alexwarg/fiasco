
#include "main.h"
#include "main_ia32_mp.h"

#include <cstdio>
#include "apic.h"
#include "app_cpu_thread.h"
#include "config.h"
#include "cpu.h"
#include "div32.h"
#include "fpu.h"
#include "globals.h"
#include "ipi.h"
#include "kernel_task.h"
#include "processor.h"
#include "per_cpu_data_alloc.h"
#include "perf_cnt.h"
#include <pfc.h>
#include "spin_lock.h"
#include "idt.h"
#include "koptions.h"

#include <cpu_id_map.h>

[[noreturn]] static void
stop_booting_ap_cpu(char const *msg, Apic_id apic_id)
{
  extern Spin_lock<Mword> _tramp_mp_spinlock;
  printf("%s, disabling CPU: %x\n", msg, cxx::int_value<Apic_id>(apic_id));
  _tramp_mp_spinlock.clear();

  while (1)
    Proc::halt();
}

inline Cpu_number
get_ap_cpu_num(bool *is_new, Apic_id apic_id)
{
  Cpu_number cpu = Apic::find_cpu(apic_id);
  if (cpu !=  Cpu_number::nil())
    {
      *is_new = false;
      return cpu;
    }

  // keep track of the last cpu ever appeared
  static Cpu_number last_cpu = Cpu_number::first();

  *is_new = true;
  if (!kernel_cpu_id_map.valid())
    return ++last_cpu;

  cpu = kernel_cpu_id_map.find(apic_id);
  if (Cpu_number::nil() == cpu)
    stop_booting_ap_cpu("Previously unknown CPU", apic_id);

  return cpu;
}

int FIASCO_FASTCALL boot_ap_cpu()
{
  Apic::activate_by_msr();

  Apic_id apic_id = Apic::get_id();
  bool cpu_is_new = false;
  Cpu_number _cpu = get_ap_cpu_num(&cpu_is_new, apic_id);

  if (cpu_is_new && !Per_cpu_data_alloc::alloc(_cpu))
    stop_booting_ap_cpu("CPU allocation failed", apic_id);

  if (cpu_is_new)
    Per_cpu_data::run_ctors(_cpu);

  Cpu &cpu = Cpu::cpus.cpu(_cpu);

  // the CPU feature flags may have changed after activating the Apic
  cpu.update_features_info();

  if (cpu_is_new)
    {
      Kmem::init_cpu(cpu);
      Apic::apic.cpu(_cpu).construct(_cpu); // do before IDT setup!
      Idt::init_current_cpu();
      Apic::init_ap();
      Ipi::init(_cpu);
    }
  else
    {
      Kmem::resume_cpu(_cpu);
      Idt::load();
      cpu.pm_resume();
      Pm_object::run_on_resume_hooks(_cpu);
    }

  Timer::init(_cpu);
  System_clock::check_ap_cpu(_cpu);

  if (cpu_is_new)
    Pfc::get()->init(_cpu);

  Perf_cnt::init_ap(cpu);


  // create kernel thread
  Kernel_thread *kernel = App_cpu_thread::may_be_create(_cpu, cpu_is_new);

  main_switch_ap_cpu_stack(kernel, !cpu_is_new);
  return 0;
}
