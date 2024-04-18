
#include <timer_omap_1mstimer.h>
#include <globalconfig.h>

#include <cassert>
#include <cstdio>

#include <config.h>
#include <kmem.h>
#include <mem_layout.h>

static void
get_timer_values_32khz(unsigned &reload, int &tpir, int &tnir)
{
  tpir   = 232000;
  tnir   = -768000;
  reload = 0xffffffe0;
  assert(Config::Scheduler_granularity == 1000); // need to adapt here
}

#ifdef CONFIG_PF_OMAP3_AM33XX

static void get_timer_values(unsigned &reload, int &tpir, int &tnir,
                             bool f_32khz)
{
  if (f_32khz)
    get_timer_values_32khz(reload, tpir, tnir);
  else
    {
      tpir   = 100000;
      tnir   = 0;
      reload = ~0 - 24 * Config::Scheduler_granularity + 1; // 24 MHz
    }
}

#endif

#if defined (CONFIG_PF_OMAP3_OMAP35XEVM) || defined(CONFIG_PF_OMAP3_BEAGLEBOARD)

static void get_timer_values(unsigned &reload, int &tpir, int &tnir, bool)
{
  get_timer_values_32khz(reload, tpir, tnir);
}

#endif

Timer_omap_1mstimer::Timer_omap_1mstimer(bool f_32khz)
: Mmio_register_block(Kmem::mmio_remap(Mem_layout::Timer1ms_phys_base, 0x100))
{
  // reset
  write<Mword>(1, TIOCP_CFG);
  while (!read<Mword>(TISTAT))
    ;
  // reset done

  // overflow mode
  write<Mword>(0x2, TIER);
  // no wakeup
  write<Mword>(0x0, TWER);

  // program timer frequency
  unsigned val;
  int tpir, tnir;
  get_timer_values(val, tpir, tnir, f_32khz);

  write<Mword>(tpir, TPIR); // gpt1, gpt2 and gpt10 only
  write<Mword>(tnir, TNIR); // gpt1, gpt2 and gpt10 only
  write<Mword>(val,  TCRR);
  write<Mword>(val,  TLDR);

  // auto-reload + enable
  write<Mword>(1 | 2, TCLR);
}


