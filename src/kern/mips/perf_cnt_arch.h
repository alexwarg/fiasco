#pragma once

#include <perf_cnt_defs.h>
#include <per_cpu_data.h>
#include <cxx/bitfield>

class Cpu;

// MIPS CP0 performance counter register access helpers.
struct Perf_cnt_arch
{
  struct Ctl_reg
  {
    Unsigned32 _v;
    Ctl_reg() = default;
    explicit Ctl_reg(Unsigned32 v) : _v(v) {}

    CXX_BITFIELD_MEMBER( 0,  0, exl, _v);
    CXX_BITFIELD_MEMBER( 1,  1, k, _v);
    CXX_BITFIELD_MEMBER( 2,  2, s, _v);
    CXX_BITFIELD_MEMBER( 3,  3, u, _v);
    CXX_BITFIELD_MEMBER( 4,  4, ie, _v);
    CXX_BITFIELD_MEMBER( 5, 12, event, _v);
    CXX_BITFIELD_MEMBER(15, 15, pctd, _v);
    CXX_BITFIELD_MEMBER(23, 24, ec, _v);
    CXX_BITFIELD_MEMBER(30, 30, w, _v);
    CXX_BITFIELD_MEMBER(31, 31, m, _v);
  };

  static bool _ec_avail;
  static bool _wide_counter;
  static Per_cpu<bool[2]> _using_odd;

  static void write_counter32(Unsigned32 v, int counter_nr);
  static void write_counter64(Mword v, int counter_nr);
  static void write_counter(Mword v, int counter_nr);
  static unsigned long read_counter32(int counter_nr);
  static unsigned long read_counter64(int counter_nr);
  static unsigned long read_counter(int counter_nr);
  static void write_ctl(Ctl_reg const &v, unsigned num);
  static Ctl_reg read_ctl(unsigned num);
};

namespace Perf_cnt
{
  void init_ap(Cpu const &);

  char const *perf_type();
  Mword get_max_perf_event();

  void get_unit_mask(Mword nr, Unit_mask_type *type,
                     Mword *default_value, Mword *nvalues);
  void get_unit_mask_entry(Mword nr, Mword idx,
                           Mword *value, const char **desc);
  void get_perf_event(Mword nr, unsigned *evntsel,
                      const char **name, const char **desc);
  void split_event(Mword event, unsigned *evntsel, Mword *unit_mask);
  void combine_event(Mword evntsel, Mword unit_mask, Mword *event);
  inline Mword lookup_event(Mword) { return 0; }

  int mode(Mword slot, const char **mode, const char **name,
           Mword *event, Mword *user, Mword *kern, Mword *edge);
  int setup_pmc(Mword slot, Mword event, Mword user, Mword kern, Mword edge);

  inline void start_watchdog() {}
  inline void stop_watchdog()  {}
  inline void touch_watchdog() {}
  inline int  have_watchdog()  { return 0; }
  inline void setup_watchdog(Mword) {}
}
