
#include <timer_realview_sp804.h>
#include <kmem.h>
#include <platform.h>

Static_object<Timer_sp804> Timer_realview_sp804::sp804;

void
Timer_realview_sp804::init(Cpu_number)
{
  sp804.construct(Kmem::mmio_remap(Mem_layout::Timer0_phys_base, 0x10));
  Platform::system_control->modify<Mword>(Platform::System_control::Timer0_enable, 0, 0);

  // all timers off
  sp804->disable();
  //Io::write<Mword>(0, Timer_sp804::Ctrl_1);
  //Io::write<Mword>(0, Timer_sp804::Ctrl_2);
  //Io::write<Mword>(0, Timer_sp804::Ctrl_3);

  sp804->reload_value(Timer_sp804::Interval);
  sp804->counter_value(Timer_sp804::Interval);
  sp804->enable(Timer_sp804::Ctrl_periodic | Timer_sp804::Ctrl_ie);
}
