
#pragma once

#include <jdb_tbuf.h>

class Jdb_tbuf_init : public Jdb_tbuf
{
private:
  static unsigned max_size();
  static unsigned allocate(unsigned size);

public:
  static void init();
};

