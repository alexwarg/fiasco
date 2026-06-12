#include <timer_realview_mptimer.h>
#include "platform_arm_realview.h"
#include <timer_sp804.h>
#include <kmem_mmio.h>
#include <rv_platforms.h>

void
Timer_realview_mptimer::init(Cpu_number cpu)
{
  Timer_sp804 timer(Kmem_mmio::map(rv_current_platform()->sp804, 0x10));
  Platform::system_control->modify<Mword>(Platform::System_control::Timer0_enable, 0, 0);

  Mword frequency = 1000000;
  Mword timer_start = ~0UL;
  unsigned factor = 5;
  Mword sp_c = timer_start - frequency / 1000 * (1 << factor);

  timer.disable();
  timer.counter_value(timer_start);
  timer.reload_value(timer_start);
  timer.enable(Timer_sp804::Ctrl_periodic);

  Mword vc = mptimer().start_as_counter();
  while (sp_c < timer.counter())
    ;
  Mword ec = mptimer().stop_counter();
  Mword interval = (vc - ec) >> factor;
  timer.disable();
  Timer_arm_mptimer::init(interval);
}
