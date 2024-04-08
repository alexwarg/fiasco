#include "entry_frame.h"

#include <cstdio>
#include "processor.h"

void Syscall_frame::dump() const
{
  printf(" R0: %08lx  R1: %08lx  R2: %08lx  R3: %08lx\n",
         r[0], r[1], r[2], r[3]);
  printf(" R4: %08lx  R5: %08lx  R6: %08lx  R7: %08lx\n",
         r[4], r[5], r[6], r[7]);
  printf(" R8: %08lx  R9: %08lx R10: %08lx R11: %08lx\n",
         r[8], r[9], r[10], r[11]);
  printf("R12: %08lx\n", r[12]);
}

