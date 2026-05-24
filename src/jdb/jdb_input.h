#pragma once

#include "types.h"
#include "l4_types.h"

class Jdb_input
{
public:
  static int get_mword(Mword *mword, int digits, int base, int first_char = 0);
  static int get_string(char *string, unsigned size);
};

