#include <feature.h>
#include <panic.h>
#include <cpu.h>

KIP_KERNEL_FEATURE("arm:hyp");

static bool _boot_cpu_has_ras;

void
Cpu::init_ras(bool is_boot_cpu)
{
  bool this_cpu_has_ras = has_ras();
  if (is_boot_cpu)
    _boot_cpu_has_ras = this_cpu_has_ras;
  else
    if (this_cpu_has_ras != _boot_cpu_has_ras)
      panic("Boot CPU %s FEAT_RAS while AP CPU %s FEAT_RAS.",
            this_cpu_has_ras ? "doesn't implement" : "implements",
            this_cpu_has_ras ? "implements" : "doesn't implement");
}

