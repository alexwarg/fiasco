
#include <cstdio>
#include "simpleio.h"

#include "jdb.h"
#include "jdb_module.h"
#include "static_init.h"
#include "types.h"

class Jdb_ipi_module : public Jdb_module
{
public:
  Jdb_ipi_module() FIASCO_INIT;

  static void print_info(Cpu_number cpu)
  {
    Ipi &ipi = Ipi::_ipi.cpu(cpu);
    printf("CPU%02u sent/rcvd: %lu/%lu\n",
           cxx::int_value<Cpu_number>(cpu), ipi._stat_sent.load(), ipi._stat_received);
  }

  Action_code action(int cmd, void *&, char const *&, int &) override
  {
    if (cmd)
      return NOTHING;

    Jdb::foreach_cpu(&print_info);

    return NOTHING;
  }

  int num_cmds() const override
  { return 1; }

  Cmd const *cmds() const override
  {
    static Cmd cs[] =
      { { 0, "", "ipi", "", "ipi\tIPI information", nullptr } };

    return cs;
  }
};

static Jdb_ipi_module jdb_ipi_module INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

Jdb_ipi_module::Jdb_ipi_module()
  : Jdb_module("INFO")
{}
