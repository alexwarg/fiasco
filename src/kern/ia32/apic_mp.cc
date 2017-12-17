#include <apic.h>
#include <apic_id.h>

#include <cstdio>

void
Apic::init_ap()
{
  init_lvt();
  init_spiv();
  init_tpr();

  disable_external_ints();

  timer_set_divisor(1);
  enable_errors();
}

int
Apic::mp_startup(Cpu const *current_cpu, Apic_id dest, bool bcast, Address tramp_page)
{
  assert((tramp_page & 0xfff00fff) == 0);

  Ipi_dest_shrt dest_shrt = bcast ? Ipi_dest_shrt::Others : Ipi_dest_shrt::Noshrt;

  reg_write(APIC_esr, 0);

  // XXX: should check for some errors after sending ipi

  // Send INIT IPI
  mp_send_ipi(dest_shrt, dest, Ipi_delivery_mode::Init, 0);

  delay(current_cpu, 200);

  // delay for 10ms (=10,000us)
  if (!mp_ipi_idle_timeout(current_cpu, 10000))
    return 1;

  // Send STARTUP IPI
  mp_send_ipi(dest_shrt, dest, Ipi_delivery_mode::Startup, tramp_page >> 12);

  // delay for 200us
  if (!mp_ipi_idle_timeout(current_cpu, 200))
    return 2;

  // Send STARTUP IPI
  mp_send_ipi(dest_shrt, dest, Ipi_delivery_mode::Startup, tramp_page >> 12);

  // delay for 200us
  if (!mp_ipi_idle_timeout(current_cpu, 200))
    return 3;

  unsigned esr = reg_read(APIC_esr);

  if (esr)
    printf("APIC status: %x\n", esr);

  return 0;
}
