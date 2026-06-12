
#include <timer_arm_rpi.h>
#include <kmem_mmio.h>
#include <mem_layout.h>

#include <cassert>

Static_object<Timer_arm_rpi> Timer_arm_rpi::_timer;

void
Timer_arm_rpi::init(Cpu_number cpu)
{
  assert (cpu == Cpu_number::boot_cpu());
  _timer.construct(Kmem_mmio::map(Mem_layout::Timer_phys_base, 0x100));
}

