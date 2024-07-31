
#include <main.h>

#include <cstdio>
#include <config.h>
#include <cpu.h>
#include <globals.h>
#include <app_cpu_thread.h>
#include <ipi.h>
#include <mips_bsp_irqs.h>
#include <per_cpu_data_alloc.h>
#include <perf_cnt.h>
#include <pfc.h>
#include <spin_lock.h>
#include <timer.h>

int boot_ap_cpu()
{
  static Cpu_number last_cpu; // keep track of the last cpu ever appeared

  Cpu_number _cpu = Cpu::cpus.find_cpu(Cpu::By_phys_id(Proc::cpu_id()));
  bool cpu_is_new = false;
  if (_cpu == Cpu_number::nil())
    {
      _cpu = ++last_cpu; // 0 is the boot cpu, so pre increment
      cpu_is_new = true;
    }

  assert (_cpu != Cpu_number::boot_cpu());

  if (cpu_is_new && !Per_cpu_data_alloc::alloc(_cpu))
    {
      extern Spin_lock<Mword> _tramp_mp_spinlock;
      printf("CPU allocation failed for CPU%u, disabling CPU.\n",
             cxx::int_value<Cpu_number>(_cpu));
      _tramp_mp_spinlock.clear();

      // FIXME: use a Platform_control API to stop the CPU
      while (1)
        Proc::halt();
    }

  if (cpu_is_new)
    Per_cpu_data::run_ctors(_cpu);

  Cpu &cpu = Cpu::cpus.cpu(_cpu);
  cpu.init(_cpu, !cpu_is_new, false);
  Pfc::get()->init(_cpu);
  Ipi::init(_cpu);
  Timer::init(_cpu);
  Perf_cnt::init_ap(cpu);
  Mips_bsp_irqs::init_ap(_cpu);
  Ipi::hw->init_ipis(_cpu, nullptr);

  // create kernel thread
  Kernel_thread *kernel = App_cpu_thread::may_be_create(_cpu, cpu_is_new);

  void *sp = kernel->init_stack();
    {
      register Mword r0 __asm__("a0") = reinterpret_cast<Mword>(kernel);
      register Mword r1 __asm__("a1") = !cpu_is_new;

      // switch to stack of kernel thread and continue thread init
      asm volatile
        ("move $29, %0            \n"  // switch stack
         "jal  call_ap_bootstrap  \n"
         :
         : "r" (sp), "r" (r0), "r" (r1));
    }
  return 0;
}
