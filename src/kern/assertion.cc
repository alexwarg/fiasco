
#include <cassert>
#include <cstdio>
#include <stdlib.h>

#include "terminate.h"

extern "C"
void
assert_fail(char const *expr_msg, char const *file, unsigned int line)
{
  printf("\nAssertion failed at %s:%u: %s\n", file, line, expr_msg);

  terminate(EXIT_FAILURE);
}
