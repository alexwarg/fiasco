
#include <timer_arm_sunxi.h>
#include <kmem.h>

Static_object<Timer_arm_sunxi::Tmr> Timer_arm_sunxi::_timer;

void
Timer_arm_sunxi::init(Cpu_number)
{
  enum { Interval = 24000000 / Config::Scheduler_granularity };
  _timer.construct(Kmem::mmio_remap(Mem_layout::Timer_phys_base, 0x100));

  _timer->write<Mword>(Interval, Tmr::addr(Tmr::TMRx_INTV_VALUE_REG));
  _timer->write<Mword>(1 | (1 << 1) | (1 << 2), Tmr::addr(Tmr::TMRx_CTRL_REG));

  _timer->write<Mword>(1 << Tmr::Timer_nr, Tmr::TMR_IRQ_EN_REG);
}

