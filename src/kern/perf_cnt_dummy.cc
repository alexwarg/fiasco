#include <perf_cnt_defs.h>

namespace Perf_cnt
{
  static Mword dummy_read_pmc() { return 0; }
  Perf_read_fn read_pmc[Max_slot] = { dummy_read_pmc, dummy_read_pmc };
}
