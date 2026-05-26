
#include <jdb_kern_info.h>
#include "static_init.h"
#include "kmem_alloc.h"
#include "kmem_slab.h"
#include <cxx/slist>

extern cxx::S_list<Kmem_slab> const &jdb_kmem_slab_list();


class Jdb_kern_info_memory : public Jdb_kern_info_module
{
public:
  Jdb_kern_info_memory()
    : Jdb_kern_info_module('m', "kmem_alloc::debug_dump")
  {
    Jdb_kern_info::register_subcmd(this);
  }

  void show() override
  {
    ((Kmem_alloc*)Kmem_alloc::allocator())->debug_dump();

    // Slab allocators
    for (auto const &&alloc: jdb_kmem_slab_list())
      alloc->debug_dump();
  }
};

static Jdb_kern_info_memory k_m INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);

