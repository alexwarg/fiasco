#pragma once

#include <tb_entry.h>
#include <string_buffer.h>
#include <types.h>

struct Log_thread_exregs : public Tb_entry
{
  Mword       id, ip, sp, op;
  void print(String_buffer *) const;
};

