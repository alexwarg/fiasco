#pragma once

#include "globals.h"
#include "jdb.h"
#include "jdb_types.h"

static void set_ipc_entry(void (*e)())
{
  typedef void (*Sys_call)(void);
  extern Sys_call sys_call_table[];
  check(!Jdb::poke_task(Jdb_address::kmem_addr(&sys_call_table[0]), &e, sizeof(e)));
}

