
#include <timer_omap3_am33xx.h>
#include <kmem_mmio.h>

enum
{
  CM_WKUP_CLKSTCTRL         = 0x00,
  CM_WKUP_TIMER0_CLKCTRL    = 0x10,
  CM_WKUP_TIMER1_CLKCTRL    = 0xc4,
  CLKSEL_TIMER1MS_CLK       = 0x28,

  CLKSEL_TIMER1MS_CLK_OSC   = 0,
  CLKSEL_TIMER1MS_CLK_32KHZ = 1,
  CLKSEL_TIMER1MS_CLK_VALUE = CLKSEL_TIMER1MS_CLK_OSC,
};

Static_object<Timer_omap_gentimer> Timer_omap3_am33xx::_timer;

void
Timer_omap3_am33xx::init_timer(Address wkup_phys)
{
  Mmio_register_block wkup(Kmem_mmio::map(wkup_phys, 0x100));
  wkup.write<Mword>(2, CM_WKUP_TIMER0_CLKCTRL);
  _timer.construct();
}


