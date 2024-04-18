
#include <timer_arm_imx21.h>
#include <kmem.h>

Timer_arm_imx21::Timer_arm_imx21(Address phys_base, unsigned long size)
: Mmio_register_block(Kmem::mmio_remap(phys_base, size))
{
  write<Mword>(0, TCTL); // Disable
  write<Mword>(TCTL_SW_RESET, TCTL); // reset timer
  for (int i = 0; i < 10; ++i)
    read<Mword>(TCN); // docu says reset takes 5 cycles

  write<Mword>(TCTL_CLKSOURCE_32kHz | TCTL_COMP_EN, TCTL);
  write<Mword>(0, TPRER);
  write<Mword>(32, TCMP);

  modify<Mword>(TCTL_TEN, 0, TCTL);
}

