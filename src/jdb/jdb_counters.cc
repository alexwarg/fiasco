
#include <cstdio>
#include <cstring>
#include "config.h"
#include "jdb_tbuf.h"
#include "jdb_module.h"
#include "kern_cnt.h"
#include "simpleio.h"
#include "static_init.h"

class Jdb_counters : public Jdb_module
{
public:
  Jdb_counters() FIASCO_INIT;

  void show()
  {
    putchar('\n');

    for (unsigned i = 0; i < Kern_cnt::Valid_ctrs; ++i)
      printf("  %-25s%10u\n", Kern_cnt::get_vld_str(i), *Kern_cnt::get_vld_ctr(i));
    putchar('\n');
  }

  void reset()
  {
    memset(Jdb_tbuf::status()->kerncnts, 0, 
           sizeof(Jdb_tbuf::status()->kerncnts));
  }

  Action_code action(int cmd, void *&, char const *&, int &) override
  {
    if (!Config::Jdb_accounting)
      {
        puts(" accounting disabled");
        return ERROR;
      }

    if (cmd == 0)
      {
        switch (counters_cmd)
          {
          case 'l':
            show();
            break;
          case 'r':
            reset();
            putchar('\n');
            break;
          }
      }
    return NOTHING;
  }

  Cmd const *cmds() const override
  {
    static Cmd cs[] =
      {
          { 0, "C", "cnt", "%c",
            "C{l|r}\tshow/reset kernel event counters", &counters_cmd },
      };
    return cs;
  }

  int num_cmds() const override
  {
    return 1;
  }

private:
  static char counters_cmd;
};

char Jdb_counters::counters_cmd;

Jdb_counters::Jdb_counters()
  : Jdb_module("MONITORING")
{}

static Jdb_counters jdb_counters INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
