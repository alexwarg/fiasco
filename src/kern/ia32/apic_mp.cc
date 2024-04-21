#include <apic.h>

void
Apic::init_ap()
{
  dump_info();
  // set some interrupt vectors to appropriate values
  init_lvt();

  // initialize APIC_spiv register
  init_spiv();

  // initialize task-priority register
  init_tpr();

  disable_external_ints();

  // get timer going on this CPU
  timer_set_divisor(1);
  enable_errors();
}

void
Apic::mp_startup(Cpu const *current_cpu, Unsigned32 dest, Address tramp_page)
{
  assert((tramp_page & 0xfff00fff) == 0);

  // XXX: should check for the apic version what to do exactly
  // XXX: should check for some errors after sending ipi

  // Send INIT IPI
  mp_send_ipi(dest, 0, APIC_IPI_INIT);

  delay(current_cpu, 200);

  // delay for 10ms (=10,000us)
  if (!mp_ipi_idle_timeout(current_cpu, 10000))
    return;

  // Send STARTUP IPI
  mp_send_ipi(dest, tramp_page >> 12, APIC_IPI_STRTUP);

  // delay for 200us
  if (!mp_ipi_idle_timeout(current_cpu, 200))
    return;

  // Send STARTUP IPI
  mp_send_ipi(dest, tramp_page >> 12, APIC_IPI_STRTUP);

  // delay for 200us
  if (!mp_ipi_idle_timeout(current_cpu, 200))
    return;
}
