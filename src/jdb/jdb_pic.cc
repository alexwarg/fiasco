
#include <cstdio>

#include "pic.h"
#include "jdb_module.h"
#include "jdb_handler_queue.h"
#include "static_init.h"
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
class Jdb_pic
  : public Jdb_module
{
private:
  static void at_enter()
  {
    pic_state = Pic::disable_all_save();
  }

  static void at_leave()
  {
    Pic::restore_all(pic_state);
  }

  void register_handlers( Jdb_handler &enter, Jdb_handler &leave )
  {
    Jdb::jdb_enter.add(&enter);
    Jdb::jdb_leave.add(&leave);
  }

public:
  Jdb_pic()
  {
    static Jdb_handler enter(at_enter);
    static Jdb_handler leave(at_leave);
    register_handlers(enter,leave);
  }

  Action_code
  action(int cmd, void *&/*args*/, char const *&/*fmt*/, int & ) override
  {
    if (cmd!=0)
      return NOTHING;

    printf("PIC state: %08x\n", pic_state);

    return NOTHING;
  }

  int num_cmds() const override
  {
    return 1;
  }

  Cmd const *cmds() const override
  {
    static Cmd cs[] =
      {{ 0, "i", "pic", "", "i\tshow pic state", nullptr }};

    return cs;
  }
};

static Jdb_pic jdb_pic INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
static Pic::Status pic_state;



