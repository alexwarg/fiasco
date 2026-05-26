
#include <jdb_module.h>

#include <perf_cnt.h>
#include <static_init.h>

#include <cstdio>

class Jdb_perf : public Jdb_module_mixin<Jdb_perf>
{
public:
  Jdb_perf() FIASCO_INIT;
  Action_code action(int cmd, void *&, char const *&, int &) override
  {
    if (cmd)
      return NOTHING;

    printf("\n");
    Mword val = Perf_cnt::read_cycle_cnt();;
    printf("Cycle counter: %08lx / %10lu\n", val, val);
    for (unsigned i = 0; i < 8; ++i)
      {
        val = Perf_cnt::read_counter(i);
        printf("Event counter %d, type=%03d: %08lx / %10lu\n",
               i, Perf_cnt::mon_event_type(i), val, val);
      }

    return NOTHING;
  }

  static cxx::static_vector<Cmd const> jdb_cmds()
  {
    static Cmd cs[] =
      {
        { 0, "M", "monperf", "", "M\tPerformance monitor events", 0 },
      };
    return cs;
  }
};


Jdb_perf::Jdb_perf()
  : Jdb_module_mixin<Jdb_perf>("INFO")
{
}

static Jdb_perf jdb_perf INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
