
#include <cstdio>

#include "irq_chip.h"
#include "irq.h"
#include "irq_mgr.h"
#include "jdb_module.h"
#include "kernel_console.h"
#include "static_init.h"
#include "thread.h"
#include "types.h"


//===================
// Std JDB modules
//===================

/**
 * 'IRQ' module.
 *
 * This module handles the 'R' command that
 * provides IRQ attachment and listing functions.
 */
class Jdb_attach_irq : public Jdb_module
{
public:
  Jdb_attach_irq() FIASCO_INIT;

  Action_code action(int cmd, void *&args, char const *&, int &) override
  {
    if (cmd)
      return NOTHING;

    if (static_cast<char*>(args) == &subcmd)
      {
        switch (subcmd)
          {
          case 'l': // list
              {
                Irq_base *r;
                putchar('\n');
                unsigned n = Irq_mgr::mgr->nr_irqs();
                for (unsigned i = 0; i < n; ++i)
                  {
                    r = static_cast<Irq*>(Irq_mgr::mgr->irq(i));
                    if (!r)
                      continue;
                    printf("IRQ %02x/%02u\n", i, i);
                  }
                return NOTHING;
              }
          }
      }
    return NOTHING;
  }

  int num_cmds() const override
  {
    return 1;
  }

  Cmd const *cmds() const override
  {
    static Cmd cs[] =
      {   { 0, "R", "irq", " [l]ist/[a]ttach: %c",
            "R{l}\tlist IRQ threads", &subcmd }
      };

    return cs;
  }

private:
  static char subcmd;
};

char Jdb_attach_irq::subcmd;
static Jdb_attach_irq jdb_attach_irq INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

Jdb_attach_irq::Jdb_attach_irq()
  : Jdb_module("INFO")
{}


#include "jdb_kobject.h"

class Jdb_kobject_irq : public Jdb_kobject_handler
{
public:
  inline Jdb_kobject_irq()
    : Jdb_kobject_handler(static_cast<Irq *>(nullptr))
  {
    Jdb_kobject::module()->register_handler(this);
  }

  bool handle_key(Kobject_common *o, int key) override
  {
    (void)o; (void)key;
    return false;
  }

  Kobject_common *follow_link(Kobject_common *o) override
  {
    Irq_sender *t = cxx::dyn_cast<Irq_sender*>(o);
    Kobject_common *k = t ? Kobject::from_dbg(Kobject_dbg::pointer_to_obj(t->owner())) : 0;
    return k ? k : o;
  }

  bool show_kobject(Kobject_common *, int) override
  { return true; }

  void show_kobject_short(String_buffer *buf, Kobject_common *o, bool) override
  {
    Irq *i = cxx::dyn_cast<Irq*>(o);
    Kobject_common *w = follow_link(o);

    buf->printf(" I=%3lx %s F=%x cnt=%u:%u",
                i->pin(), i->chip()->chip_type(),
                static_cast<unsigned>(i->flags()), i->cnt(), i->xcpu_cnt());

    i->dbg_print(buf, w != o ? w : nullptr);
  }
};

static
bool
filter_irqs(Kobject_common const *o)
{ return cxx::dyn_cast<Irq const *>(o); }

static Jdb_kobject_list::Mode INIT_PRIORITY(JDB_MODULE_INIT_PRIO) tnt("[IRQs]", filter_irqs);

static Jdb_kobject_irq jdb_kobject_irq INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
