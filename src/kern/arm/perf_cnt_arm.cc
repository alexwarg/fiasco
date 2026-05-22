
#include <perf_cnt.h>
#include <perf_cnt_defs.h>
#include <cstdio>
#include "static_init.h"
#include "tb_entry.h"

namespace Perf_cnt
{

static Mword dummy_read_pmc() { return 0; }

Perf_read_fn read_pmc[Max_slot] =
{ dummy_read_pmc, dummy_read_pmc };

static Mword read_counter_0()
{ return read_counter(0); }

static Mword read_counter_1()
{ return read_counter(1); }

void get_unit_mask(Mword, Unit_mask_type *type, Mword *, Mword *)
{
  *type = Perf_cnt::Fixed;
}


void get_unit_mask_entry(Mword, Mword, Mword *value, const char **desc)
{
  *value = 0;
  *desc  = 0;
}


void get_perf_event(Mword nr, unsigned *evntsel,
                    const char **name, const char **desc)
{
  // having one set of static strings in here should be ok
  static char _name[20];
  static char _desc[50];

  snprintf(_name, sizeof(_name), "Event_%lx", nr);
  _name[sizeof(_name) - 1] = 0;

  snprintf(_desc, sizeof(_desc), "Check manual for description of event %lx", nr);
  _desc[sizeof(_desc) - 1] = 0;

  *name = (const char *)&_name;
  *desc = (const char *)&_desc;
  *evntsel = nr;
}

void split_event(Mword event, unsigned *evntsel, Mword *)
{
  *evntsel = event;
}

Mword lookup_event(Mword) { return is_avail() ? 0 : (Mword)-1; }

void combine_event(Mword evntsel, Mword, Mword *event)
{
  *event = evntsel;
}


static FIASCO_INIT_CPU
void init()
{
  init_cpu();

  read_pmc[0] = read_counter_0;
  read_pmc[1] = read_counter_1;

  // Don't use PMCCNT_EL0 as time stamp counter.
  // Use the default generic ARM timer instead.
  // Tb_entry::set_cycle_read_func(read_cycle_cnt);
}

int mode(Mword slot, const char **mode, const char **name,
         Mword *event, Mword *user, Mword *kern, Mword *edge)
{
  static char _n[Max_slot][5];

  if (slot >= Max_slot)
    return 0;

  if (!is_avail())
    return 0;

  *event = mon_event_type(slot);

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

  set_event_type(slot, event);

  Tb_entry::set_rdcnt(slot, read_pmc[slot]);

  return 1;
}

}

STATIC_INITIALIZE_P(Perf_cnt, PERF_CNT_INIT_PRIO);

