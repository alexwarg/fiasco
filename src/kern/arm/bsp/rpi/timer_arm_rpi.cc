
#include <timer_arm_rpi.h>
#include <kmem.h>

#include <cassert>

Static_object<Timer_arm_rpi> Timer_arm_rpi::_timer;

void
Timer_arm_rpi::init(Cpu_number cpu)
{
  assert (cpu == Cpu_number::boot_cpu());
  _timer.construct(Kmem::mmio_remap(Mem_layout::Timer_phys_base, 0x100));
}

