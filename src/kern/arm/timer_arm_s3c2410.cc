
#include <timer_arm_s3c2410.h>
#include <kmem_mmio.h>

Static_object<Timer_arm_s3c2410> Timer_arm_s3c2410::_timer;

Timer_arm_s3c2410::Timer_arm_s3c2410(Address phys_base, bool tint_cstat, Mword reload_value)
  : Mmio_register_block(Kmem_mmio::map(phys_base, 0x100))
{
  write<Mword>(0, TCFG0); // prescaler config
  write<Mword>(0, TCFG1); // mux select
  write<Mword>(reload_value, TCNTB0  + Timer_nr * 0xc); // reload value
  write<Mword>(reload_value, TCMPB0  + Timer_nr * 0xc); // reload value

  unsigned shift = Timer_nr == 0 ? 0 : (Timer_nr * 4 + 4);
  write<Mword>(5 << shift, TCON); // start + autoreload

  if (tint_cstat)
    write<Mword>(1 << Timer_nr, TINT_CSTAT);
}


