
#include <jdb_kern_info.h>
#include <kip.h>
#include <static_init.h>

class Jdb_kern_info_kip : public Jdb_kern_info_module
{
public:
  Jdb_kern_info_kip()
    : Jdb_kern_info_module('f', "Kernel Interface Page (KIP)")
  {
    Jdb_kern_info::register_subcmd(this);
  }

  void show() override
  {
    Kip::k()->print();
  }

};

static Jdb_kern_info_kip k_f INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);


