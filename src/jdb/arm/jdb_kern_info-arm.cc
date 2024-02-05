
#include <cstdio>
#include <cstring>

#include <config.h>
#include <globals.h>
#include <space.h>

#include <jdb_kern_info.h>

class Jdb_kern_info_misc : public Jdb_kern_info_module
{
public:
  Jdb_kern_info_misc()
    : Jdb_kern_info_module('i', "Miscellaneous info")
  {
    Jdb_kern_info::register_subcmd(this);
  }

  void show() override
  {
    // FIXME: assume UP here (current_mem_space(0))
    Cpu_time clock = Kip::k()->clock();
    auto pdir = Mem_space::current_mem_space(Cpu_number::boot_cpu())->dir();
    printf("clck: %08llx.%08llx\npdir: %08lx\n",
           clock >> 32, clock & 0xffffffffU, reinterpret_cast<unsigned long>(pdir));
  }
};


static Jdb_kern_info_misc k_i INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);

