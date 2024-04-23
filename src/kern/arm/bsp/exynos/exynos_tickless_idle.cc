#include <cpu_tickless_idle.h>
#include <kernel_thread.h>
#include <globalconfig.h>
#include <scheduler_iface.h>
#include <platform_control.h>
#include <cpu.h>

struct Exynos_tickless_idle : Cpu_tickless_idle_default
{
#ifdef CONFIG_MP
  void arch_tickless_idle(Cpu_number cpu)
  {
    if (cpu != Cpu_number::boot_cpu() && Platform_control::cpu_suspend_allowed(cpu))
      {
        take_cpu_offline(cpu);
        Scheduler_iface::root()->trigger_hotplug_event();

        Platform_control::do_core_n_off(cpu);

        take_cpu_online(cpu);
        Scheduler_iface::root()->trigger_hotplug_event();
      }
    else
      Proc::halt();
  }
#endif
};

static Cpu_tickless_idle<Exynos_tickless_idle> exynos_tickless_idle;

[[gnu::constructor]]
static void exynos_tickless_idle_init()
{
  Kernel_thread::idle = &exynos_tickless_idle;
}

