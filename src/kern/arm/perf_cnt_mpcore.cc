#include <perf_cnt_defs.h>
#include <perf_cnt_arch_mpcore.h>
#include <static_init.h>
#include <tb_entry.h>

#include <cstdio>

namespace Perf_cnt
{

static Mword dummy_read_pmc() { return 0; }

Perf_read_fn read_pmc[Max_slot] = { dummy_read_pmc, dummy_read_pmc };

static char const *const perf_type_str = "MP-C";

Unsigned64 read_cycle_cnt()
{ return Perf_cnt_arch_mpcore::read_cycle_cnt(); }

unsigned long read_counter(int counter_nr)
{ return Perf_cnt_arch_mpcore::read_counter(counter_nr); }

unsigned mon_event_type(int nr)
{ return Perf_cnt_arch_mpcore::mon_event_type(nr); }

static Mword read_counter_0() { return Perf_cnt_arch_mpcore::read_counter(0); }
static Mword read_counter_1() { return Perf_cnt_arch_mpcore::read_counter(1); }

static FIASCO_INIT_CPU
void init()
{
  Perf_cnt_arch_mpcore::init_cpu(*Cpu::boot_cpu());
  read_pmc[0] = read_counter_0;
  read_pmc[1] = read_counter_1;
}

char const *perf_type()
{ return perf_type_str; }

Mword get_max_perf_event()
{ return Perf_cnt_arch_mpcore::get_max_perf_event(); }

void get_unit_mask(Mword, Unit_mask_type *type, Mword *, Mword *)
{
  *type = Fixed;
}

void get_unit_mask_entry(Mword, Mword, Mword *value, const char **desc)
{
  *value = 0;
  *desc  = nullptr;
}

void get_perf_event(Mword nr, unsigned *evntsel,
                    const char **name, const char **desc)
{
  static char _name[20];
  static char _desc[50];

  snprintf(_name, sizeof(_name), "Event_%lx", nr);
  _name[sizeof(_name) - 1] = 0;

  snprintf(_desc, sizeof(_desc), "Check manual for description of event %lx", nr);
  _desc[sizeof(_desc) - 1] = 0;

  *name    = _name;
  *desc    = _desc;
  *evntsel = nr;
}

void split_event(Mword event, unsigned *evntsel, Mword *)
{
  *evntsel = event;
}

Mword lookup_event(Mword) { return 0; }

void combine_event(Mword evntsel, Mword, Mword *event)
{
  *event = evntsel;
}

int mode(Mword slot, const char **mode, const char **name,
         Mword *event, Mword *user, Mword *kern, Mword *edge)
{
  static char _n[Max_slot][5];

  if (slot >= Max_slot)
    return 0;

  *event = Perf_cnt_arch_mpcore::mon_event_type(slot);

  snprintf(_n[slot], sizeof(_n[slot]), "e%lx", *event & 0xfff);
  _n[slot][sizeof(_n[slot]) - 1] = 0;
  *name = _n[slot];

  *mode = "on";
  *user = *kern = *edge = 0;

  return 1;
}

int setup_pmc(Mword slot, Mword event, Mword, Mword, Mword)
{
  if (slot >= Max_slot)
    return 0;

  Perf_cnt_arch_mpcore::set_event_type(slot, event);
  Tb_entry::set_rdcnt(slot, read_pmc[slot]);

  return 1;
}

void start_watchdog() {}
void stop_watchdog()  {}
void touch_watchdog() {}
int  have_watchdog()  { return 0; }
void setup_watchdog(Mword) {}

} // namespace Perf_cnt

STATIC_INITIALIZE_P(Perf_cnt, PERF_CNT_INIT_PRIO);
