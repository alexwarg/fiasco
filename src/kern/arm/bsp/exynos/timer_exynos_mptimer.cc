#include <timer.h>
#include <timer_mct.h>
#include <kmem.h>

#include <cstdio>

void
Timer::init(Cpu_number)
{
  Mct_timer mct(Kmem::mmio_remap(Mem_layout::Mct_phys_base, 0x100));
  // probably need to select proper clock source for MCT
  Mword timer_start = 0UL;
  unsigned factor = 5;
  Mword sp_c = timer_start
               + Mct_core_timer::Mct_freq / (1000000 / Config::Scheduler_granularity)
	         * (1 << factor);

  mct.write<Mword>(0, Mct_timer::Reg::Cfg);
  mct.write<Mword>(1 << 8, Mct_timer::Reg::Tcon);

  mct.write<Mword>(0, Mct_timer::Reg::Cnt_u);
  mct.write<Mword>(timer_start, Mct_timer::Reg::Cnt_l);
  Mword vc = start_as_counter();
  while (sp_c > mct.read<Mword>(Mct_timer::Reg::Cnt_l))
    ;
  Mword interval = (vc - stop_counter()) >> factor;

  mct.write<Mword>(0, Mct_timer::Reg::Tcon);

  if (0)
    printf("MP-Timer-Interval: %ld\n", interval);

  Timer_arm_mptimer::init(interval);
}
