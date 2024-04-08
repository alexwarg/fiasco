#include "entry_frame.h"
#include <cstdio>

void
Syscall_frame::dump() const
{
  printf(" R0: %08lx  R1: %08lx  R2: %08lx  R3: %08lx\n",
         r[0], r[1], r[2], r[3]);
  printf(" R4: %08lx\n", r[4]);
}


