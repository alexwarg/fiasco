#pragma once

#include <jdb_types.h>

class Jdb_bp
{
public:
  static int instruction_bp_at_addr(Jdb_address) { return 0; }
};
