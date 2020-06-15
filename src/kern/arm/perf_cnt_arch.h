#pragma once

#include <globalconfig.h>
#include "initcalls.h"
#include "types.h"

class Cpu;

#if defined(CONFIG_ARM_V7) || defined(CONFIG_ARM_V8)
#include <perf_cnt_arm_v7plus.h>
namespace Perf_cnt {
  using namespace Perf_cnt_arm_v7plus;
}
#elif defined (CONFIG_ARM_MPCORE)
#include <perf_cnt_arm_mpcore.h>
namespace Perf_cnt {
  using namespace Perf_cnt_arch_mpcore;
}
#else
// Common ARM PMUv1/v2 implementation (ARMv7 and ARMv8).
namespace Perf_cnt
{
  inline Unsigned64 read_cycle_cnt() { return 0; }
  inline unsigned mon_event_type(int) { return 0; }
  inline unsigned long read_counter(int) { return 0; }
  inline char const *perf_type() { return "none"; }
  // Default: no events known.  CPU-specific subclasses override this.
  inline Mword get_max_perf_event() { return 0; }
  inline bool is_avail() { return true; }
  inline void set_event_type(int, int) {}

  inline void init_cpu(Cpu const &) {}
  Unsigned64 read_cycle_cnt();
  unsigned long read_counter(int counter_nr);
  unsigned mon_event_type(int nr);
};

#endif

namespace Perf_cnt
{
  void get_unit_mask(Mword, Unit_mask_type *type, Mword *, Mword *);
  void get_unit_mask_entry(Mword, Mword, Mword *value, const char **desc);
  void get_perf_event(Mword nr, unsigned *evntsel,
                      const char **name, const char **desc);
  void split_event(Mword event, unsigned *evntsel, Mword *);
  Mword lookup_event(Mword);
  void combine_event(Mword evntsel, Mword, Mword *event);
  int mode(Mword slot, const char **mode, const char **name,
           Mword *event, Mword *user, Mword *kern, Mword *edge);
  int setup_pmc(Mword slot, Mword event, Mword, Mword, Mword);

  inline void init_ap(Cpu const &cpu)
  { init_cpu(cpu); }
}
