
#include <timer_omap3.h>
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

Static_object<Timer_omap_1mstimer> Timer_omap3::_timer;

void
Timer_omap3::init_am33xx(Address wkup_phys, Address clksel_phys)
{
  Mmio_register_block wkup(Kmem_mmio::map(wkup_phys, 0x100));
  Mmio_register_block clksel(Kmem_mmio::map(clksel_phys, 0x100));

  // enable DMTIMER1_1MS
  wkup.write<Mword>(2, CM_WKUP_TIMER1_CLKCTRL);
  wkup.read<Mword>(CM_WKUP_TIMER1_CLKCTRL);
  clksel.write<Mword>(CLKSEL_TIMER1MS_CLK_VALUE, CLKSEL_TIMER1MS_CLK);
  for (int i = 0; i < 1000000; ++i) // instead, poll proper reg
    asm volatile("" : : : "memory");

  _timer.construct(CLKSEL_TIMER1MS_CLK_VALUE == CLKSEL_TIMER1MS_CLK_32KHZ);
}

void
Timer_omap3::init_35xx(Address wkup_phys)
{
  // select 32768 Hz input to GPTimer1 (timer1 only!)
  Mmio_register_block(Kmem_mmio::map(wkup_phys, 0x10)).modify(0, 1, 0);
  _timer.construct(true);
}
