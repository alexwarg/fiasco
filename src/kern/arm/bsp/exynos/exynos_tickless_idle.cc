#include <cpu_tickless_idle.h>
#include <kernel_thread.h>
#include <globalconfig.h>
#include <pfc.h>

struct Exynos_tickless_idle : Cpu_tickless_idle_default
{
#ifdef CONFIG_MP
  void arch_tickless_idle(Cpu_number cpu)
  {
    if (!Pfc::get()->power_down_cpu(cpu))
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

