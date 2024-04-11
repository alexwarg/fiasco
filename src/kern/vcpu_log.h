#pragma once

#include <tb_entry.h>
#include <string_buffer.h>

struct Vcpu_log : public Tb_entry
{
  Mword state;
  Mword ip;
  Mword sp;
  Mword space;
  Mword err;
  unsigned char type;
  unsigned char trap;
  void print(String_buffer *buf) const;
};

