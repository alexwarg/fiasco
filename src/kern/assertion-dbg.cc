
#include <cassert>
#include <cstdio>
#include <stdlib.h>

#include "kernel_console.h"
#include "thread.h"

extern "C"
void assert_fail(char const *expr_msg, char const *file, unsigned int line,
                 void const *caller);

void
assert_fail(char const *expr_msg, char const *file, unsigned int line,
            void const *caller)
{
  // make sure that GZIP mode is off
  Kconsole::console()->end_exclusive(Console::GZIP);

  printf("\nAssertion failed at %s:%u:%p: %s\n", file, line, caller, expr_msg);

  Thread::system_abort();
}

