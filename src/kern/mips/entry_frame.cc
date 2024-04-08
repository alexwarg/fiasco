#include "entry_frame.h"

#include <cstdio>
#include "cp0_status.h"

void
Return_frame::dump() const
{
  char const *const regs[] =
  {
    "00", "at", "v0", "v1",
    "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3",
    "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3",
    "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1",
    "gp", "sp", "fp", "ra"
  };

  int sz = 2 * sizeof(Mword);
  printf("00[ 0]: %0*x ", sz, 0);

  for (unsigned i = 1; i < 32; ++i)
    printf("%s[%2d]: %0*lx%s", regs[i], i, sz, r[i], (i & 3) == 3 ? "\n" : " ");

  printf("HI: %*lx LO: %*lx\n", sz, hi, sz, lo);
  printf("Status %0*lx Cause %0*lx EPC %0*lx\n", sz, status, sz, cause, sz, epc);
  //printf("Cause  %0*lx BadVaddr %0*lx\n", sz, cause, sz, badvaddr);
}
