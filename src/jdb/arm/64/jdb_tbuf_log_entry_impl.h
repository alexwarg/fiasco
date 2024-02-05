
#pragma once

#include <jdb_tbuf.h>
#include <tb_entry_generic.h>
#include <mem_unit.h>

unsigned char
Jdb_tbuf::get_entry_status(Tb_log_table_entry const *e)
{
  return (*reinterpret_cast<Unsigned32 const *>(e->patch) >> 5) & 0xffff;
}

void
Jdb_tbuf::set_entry_status(Tb_log_table_entry const *e,
                           unsigned char value)
{
  Unsigned32 *insn = reinterpret_cast<Unsigned32 *>(e->patch);
  *insn = (*insn & ~(0xffffU << 5)) | (Unsigned32{value} << 5);
  Mem_unit::make_coherent_to_pou(insn, 4);
}

