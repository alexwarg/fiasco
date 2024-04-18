
#include <timer_mct.h>

void
Mct_timer::start_free_running()
{
  write<Mword>(0, Reg::Cnt_u);
  write<Mword>(0, Reg::Cnt_l);
  modify<Mword>(1 << 8, 0, Reg::Tcon);
}

void
Mct_core_timer::configure()
{
  write<Mword>(1, Reg::L_TCNTB);
  wstat_poll(1);
  set_interval(Interval);

  // run timer
  write<Mword>(1, Reg::L_TCON);
  wstat_poll(8);

  // enable interrupt
  write<Mword>(1, Reg::L_INT_ENB);
  write<Mword>(7, Reg::L_TCON);
  wstat_poll(8);
}

