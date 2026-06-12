
#include <timer_arm_kirkwood.h>
#include <kmem_mmio.h>
#include <mem_layout.h>

Static_object<Timer_arm_kirkwood> Timer_arm_kirkwood::_timer;

Timer_arm_kirkwood::Timer_arm_kirkwood()
: Mmio_register_block(Kmem_mmio::map(Mem_layout::Timer_phys_base, 0x400))
{
  // Disable timer
  write(0, Control_Reg);

  // Set current timer value and reload value
  write<Mword>(Reload_value, Timer0_Reg);
  write<Mword>(Reload_value, Reload0_Reg);

  modify<Mword>(Timer0_enable | Timer0_auto, 0, Control_Reg);
  modify<Unsigned32>(Timer0_bridge_num, 0, Bridge_mask);
}


