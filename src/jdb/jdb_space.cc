
#include <climits>
#include <cstring>
#include <cstdio>

#include "jdb.h"
#include "jdb_core.h"
#include "jdb_module.h"
#include "jdb_screen.h"
#include "jdb_kobject.h"
#include "kernel_console.h"
#include "kernel_task.h"
#include "keycodes.h"
#include "ram_quota.h"
#include "simpleio.h"
#include "task.h"
#include "thread.h"
#include "static_init.h"

class Jdb_space : public Jdb_module, public Jdb_kobject_handler
{
public:
  Jdb_space() FIASCO_INIT;

  bool show_kobject(Kobject_common *o, int lvl) override
  {
    Task *t = cxx::dyn_cast<Task*>(o);
    show(t);
    if (lvl)
      {
        Jdb::getchar();
        return true;
      }

    return false;
  }

  void show_kobject_short(String_buffer *buf, Kobject_common *o, bool) override
  {
    Task *t = cxx::dyn_cast<Task*>(o);
    if (t == Kernel_task::kernel_task())
      buf->printf(" {KERNEL}");

    buf->printf(" R=%ld", t->ref_cnt());
  }

  Action_code action(int cmd, void *&, char const *&, int &) override;
  Cmd const *cmds() const override;
  int num_cmds() const override;

private:
  static Task *task;

  static void print_space(Space *s)
  {
    printf("%p", static_cast<void *>(s));
  }

  void show(Task *t)
  {
    Jdb::cursor(3, 1);
    Jdb::line();
    printf("\nSpace %p (Kobject*)%p%s\n", static_cast<void *>(t),
           static_cast<void *>(static_cast<Kobject*>(t)),
           Jdb::clear_to_eol_str());

    for (Space::Ku_mem_list::Const_iterator m = t->_ku_mem.begin(); m != t->_ku_mem.end();
         ++m)
      printf("  utcb area: user_va=%p kernel_va=%p size=%x%s\n",
             m->u_addr.get(), m->k_addr, m->size, Jdb::clear_to_eol_str());

    unsigned long m = t->ram_quota()->current();
    printf("  mem usage: %lu (%luKB) ", m, m/1024);
    if (t->ram_quota()->unlimited())
      printf("-- unlimited%s\n", Jdb::clear_to_eol_str());
    else
      {
        unsigned long l = t->ram_quota()->limit();
        printf("of %lu (%luKB) @%p%s\n", l, l/1024,
               static_cast<void *>(t->ram_quota()), Jdb::clear_to_eol_str());
      }
    Jdb::line();
  }

};

Task *Jdb_space::task;

Jdb_space::Jdb_space()
  : Jdb_module("INFO"), Jdb_kobject_handler(static_cast<Task *>(nullptr))
{
  Jdb_kobject::module()->register_handler(this);
}

static bool space_filter(Kobject_common const *o)
{ return cxx::dyn_cast<Task const *>(o); }

static bool filter_task_thread(Kobject_common const *o)
{
  return cxx::dyn_cast<Task const *>(o) || cxx::dyn_cast<Thread const *>(o);
}

Jdb_module::Action_code
Jdb_space::action(int cmd, void *&, char const *&, int &)
{
  if (cmd == 0)
    {
      Jdb_kobject_list list(space_filter);
      list.do_list();
    }
  return NOTHING;
}

Jdb_module::Cmd const *
Jdb_space::cmds() const
{
  static Cmd cs[] =
    {
        { 0, "s", "spacelist", "", "s\tshow task list", nullptr },
    };
  return cs;
}

int
Jdb_space::num_cmds() const
{ return 1; }

static Jdb_space jdb_space INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
static Jdb_kobject_list::Mode INIT_PRIORITY(JDB_MODULE_INIT_PRIO) tnt("[Tasks + Threads]", filter_task_thread);

