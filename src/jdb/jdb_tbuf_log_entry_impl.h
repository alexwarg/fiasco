
#pragma once

#include <jdb_tbuf.h>
#include <tb_entry_generic.h>
#include <mem_unit.h>


unsigned char
Jdb_tbuf::get_entry_status(Tb_log_table_entry const *e)
{
  return *(e->patch);
}

void
Jdb_tbuf::set_entry_status(Tb_log_table_entry const *e,
                           unsigned char value)
{
  *(e->patch) = value;
  Mem_unit::make_coherent_to_pou(e->patch, sizeof(*(e->patch)));
}
