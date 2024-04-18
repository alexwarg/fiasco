#include <timer_arm_imx_timrot.h>
#include <kmem.h>

Timer_imx_timrot::Timer_imx_timrot(Address phys, unsigned size)
: _reg(Kmem::mmio_remap(phys, size))
{
  _reg[HW_TIMROT_TIMCTRL0] = 0;
  _reg[HW_TIMROT_TIMCTRL0_SET]
    = CTRL_SELECT_32KHZ | CTRL_RELOAD | CTRL_UPDATE | CTRL_IRQ_EN;

  Unsigned32 v = 32000 / (1000000 / Config::Scheduler_granularity);
  _reg[HW_TIMROT_FIXED_COUNT0] = v;
}

