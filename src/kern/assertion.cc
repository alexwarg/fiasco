
#include <cassert>
#include <cstdio>
#include <stdlib.h>

#include "terminate.h"

extern "C"
void
assert_fail(char const *expr_msg, char const *file, unsigned int line,
            void *caller)
{
  printf("\nAssertion failed at %s:%u:%p: %s\n", file, line, caller, expr_msg);

  terminate(EXIT_FAILURE);
}
