#pragma once

#include "globalconfig.h"

#if defined (CONFIG_JDB)
#include "kdb_ke_arch.h"
#else

#include <cstdio>

inline
void kdb_ke(char const *msg)
{
  printf("NO JDB: %s\n"
         "So go ahead.\n", msg);
}

inline void kdb_ke_sequence(char const *, unsigned) {}

#endif
