#pragma once

#include <cpu.h>
#include <perf_cnt_arm_v7plus_bits.h>

namespace Perf_cnt_arm_v7plus
{
  extern int _nr_counters;
  using namespace Perf_cnt_arm_v7plus_bits;
  enum
  {
    PMNC_ENABLE     = 1 << 0,
    PMNC_PERF_RESET = 1 << 1,
    PMNC_CNT_RESET  = 1 << 2,
  };

  inline bool is_avail()
  {
    if (!_nr_counters)
      return false;

    switch (Cpu::boot_cpu()->copro_dbg_model())
      {
      case Cpu::Copro_dbg_model_v7:
      case Cpu::Copro_dbg_model_v7_1:
      case Cpu::Copro_dbg_model_v8:
      case Cpu::Copro_dbg_model_v8_plus_vhe:
      case Cpu::Copro_dbg_model_v8_2:
      case Cpu::Copro_dbg_model_v8_4: return true;
      default: return false;
      }
  }

  inline void set_event_type(int counter_nr, int event)
  {
    if (!is_avail())
      return;
    pmnxsel(counter_nr);
    evtsel(event);
  }

  inline Unsigned64 read_cycle_cnt()
  {
    if (!is_avail())
      return 0;
    return ccnt();
  }

  inline unsigned long read_counter(int counter_nr)
  {
    if (!is_avail())
      return 0;
    if (counter_nr >= _nr_counters)
      return ccnt();
    pmnxsel(counter_nr);
    return pmcnt();
  }

  inline unsigned mon_event_type(int nr)
  {
    if (!is_avail())
      return 0;
    if (nr >= _nr_counters)
      return 0;
    pmnxsel(nr);
    return evtsel();
  }

  inline void init_cpu()
  {
    if (!is_avail())
      return;

    _nr_counters = (pmcr() >> 11) & 0x1f;
    pmcr(PMNC_ENABLE | PMNC_PERF_RESET | PMNC_CNT_RESET);
    cntens((1ul << 31) | ((1ul << _nr_counters) - 1));
    useren(IS_ENABLED(CONFIG_PERF_CNT_USER) ? 1 : 0);
  }

inline char const *perf_type() { return "ACor"; }

#if defined(CONFIG_ARM_CORTEX_A8)
  inline Mword get_max_perf_event()
  { return is_avail() ? 0x73 : 0; }
#elif defined(CONFIG_ARM_CORTEX_A9)
  inline Mword get_max_perf_event()
  { return is_avail() ? 0x94 : 0; }
#  elif defined(CONFIG_ARM_CORTEX_A15)
  inline Mword get_max_perf_event()
  { return is_avail() ? 0x7f : 0; }
#else
  inline Mword get_max_perf_event()
  { return 0; }
#endif

};

