#pragma once

#include <globalconfig.h>
#include <types.h>
#include <perf_cnt_defs.h>
#ifdef CONFIG_PERF_CNT
#include <perf_cnt_arch.h>
#else

namespace Perf_cnt
{
  inline void get_unit_mask(Mword, Unit_mask_type *, Mword *, Mword *) {}
  inline void get_unit_mask_entry(Mword, Mword, Mword *, const char **) {}
  inline void get_perf_event(Mword, unsigned *, const char **, const char **) {}
  inline Mword get_max_perf_event() { return 0; }
  inline void split_event(Mword, unsigned *, Mword *) {}
  inline Mword lookup_event(Mword) { return 0; }
  inline void combine_event(Mword, Mword, Mword *) {}
  inline char const *perf_type() { return "nothing"; }
  inline int mode(Mword /*slot*/, const char **mode, const char **name,
                  Mword *event, Mword *user, Mword *kern, Mword *edge)
  {
    *mode  = *name = "";
    *user  = *kern = *edge = *event = 0;
    return 0;
  }

  inline int setup_pmc(Mword, Mword, Mword, Mword, Mword)
  { return 0; }

  inline void start_watchdog() {}
  inline void stop_watchdog() {}
  inline void touch_watchdog() {}
  inline int  have_watchdog() { return 0; }
  inline void setup_watchdog(Mword) {}
  static inline void init_ap(Cpu &) {}
}
#endif
