
#include <timer_arm_imx_epit.h>
#include <kmem_mmio.h>

Timer_arm_imx_epit::Timer_arm_imx_epit(Address phys_base)
: Mmio_register_block(Kmem_mmio::map(phys_base, 0x100))
{
  write<Mword>(0, EPITCR); // Disable
  write<Mword>(EPITCR_SWR, EPITCR);
  while (read<Mword>(EPITCR) & EPITCR_SWR)
    ;

  write<Mword>(EPITSR_OCIF, EPITSR);

  write<Mword>(EPITCR_CLKSRC_IPG_CLK_32K
               | (0 << EPITCR_PRESCALER_SHIFT)
               | EPITCR_WAITEN
               | EPITCR_RLD
               | EPITCR_OCIEN
               | EPITCR_ENMOD,
               EPITCR);

  write<Mword>(0, EPITCMPR);

  write<Mword>(32, EPITLR);

  modify<Mword>(EPITCR_ENABLE, 0, EPITCR);
}


