#pragma once

#include "cpu.h"
#include "initcalls.h"
#include "types.h"

// ARM MPCore SCU performance monitor.
// The MPCore has no CP15 PMU; counters are in the SCU memory-mapped block.
namespace Perf_cnt_arch_mpcore
{
  enum {
    CPU_CONTROL = 0x00,
    CONFIG      = 0x04,
    CPU_STATUS  = 0x08,
    MON_CONTROL = 0x10,

    MON_CONTROL_ENABLE = 1,
    MON_CONTROL_RESET  = 2,

    EVENT_DISABLED              = 0,
    EVENT_EXTMEM_CPU0           = 1,
    EVENT_EXTMEM_CPU1           = 2,
    EVENT_EXTMEM_CPU2           = 3,
    EVENT_EXTMEM_CPU3           = 4,
    EVENT_OTHER_CACHE_HIT_CPU0  = 5,
    EVENT_OTHER_CACHE_HIT_CPU1  = 6,
    EVENT_OTHER_CACHE_HIT_CPU2  = 7,
    EVENT_OTHER_CACHE_HIT_CPU3  = 8,
    EVENT_NON_PRESENT_CPU0      = 9,
    EVENT_NON_PRESENT_CPU1      = 10,
    EVENT_NON_PRESENT_CPU2      = 11,
    EVENT_NON_PRESENT_CPU3      = 12,
    EVENT_LINE_MIGRATION        = 13,
    EVENT_READ_BUSY_MASTER0     = 14,
    EVENT_READ_BUSY_MASTER1     = 15,
    EVENT_WRITE_BUSY_MASTER0    = 16,
    EVENT_WRITE_BUSY_MASTER1    = 17,
    EVENT_EXTMEM_TRANSFER_READ  = 18,
    EVENT_EXTMEM_TRANSFER_WRITE = 19,
    EVENT_CYCLE_COUNT           = 31,
  };

  inline Address mon_event_type_addr(int nr) { return 0x14 + nr; }
  inline Address mon_counter(int nr)         { return 0x1c + nr * 4; }

  inline bool is_avail() { return true; }
  inline Mword get_max_perf_event() { return 32; }

  inline void set_event_type(int counter_nr, int event)
  { Cpu::scu.r.r<8>(mon_event_type_addr(counter_nr)) = event; }

  inline Unsigned64 read_cycle_cnt()
  { return read_counter(7); }

  inline unsigned long read_counter(int counter_nr)
  { return Cpu::scu.r[mon_counter(counter_nr)]; }

  inline unsigned mon_event_type(int nr)
  { return Cpu::scu.r.r<8>(mon_event_type_addr(nr)); }

  inline Mword get_max_perf_event() { return is_avail() ? 32 : 0; }

  inline char const *perf_type() { return "MP-C"; }

  inline void init_cpu(Cpu const &)
  {
    static_assert(Scu::Available, "No SCU available in this configuration");
    Cpu::scu.r[MON_CONTROL] = 0xff << 16 | MON_CONTROL_RESET | MON_CONTROL_ENABLE;
    set_event_type(7, EVENT_CYCLE_COUNT);
  }
};
