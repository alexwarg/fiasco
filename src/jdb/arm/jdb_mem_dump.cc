
#include <cstdio>

#include "irq_alloc.h"
#include "jdb_module.h"
#include "static_init.h"
#include "types.h"


//===================
// Std JDB modules
//===================

/**
 * 'Mem-Dump' module.
 * 
 * This module handles the 'd' command that
 * dumps the memory at the specified address.
 */
class Jdb_mem_dump
  : public Jdb_module
{
public:
  Action_code action( int cmd, void *&, char const *&, int & ) override
  int const num_cmds() const override;
  Cmd const *const cmds() const override;

private:
  char subcmd;
  void *address;
};

static Jdb_mem_dump jdb_mem_dump INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

Jdb_module::Action_code Jdb_mem_dump::action( int cmd, void *&, char const *&, int & )
{
  if(cmd!=0)
    return NOTHING;

  unsigned v = *reinterpret_cast<unsigned*>(reinterpret_cast<Mword>(address) & ~0x03);
  printf(" => 0x%08x\n", v);
  return NOTHING;
}

int const Jdb_mem_dump::num_cmds() const
{
  return 1;
}

Jdb_module::Cmd const *const Jdb_mem_dump::cmds() const
{
  static Cmd cs[] =
    {  { 0, "d", "dump", " address: %x", 
	   "d\tdump memory at specific address", static_cast<void*>(&address) }
    };

  return cs;
}

