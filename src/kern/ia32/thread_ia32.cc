
#include <thread.h>
#include <cstdio>

void
Thread::print_page_fault_error(Mword e)
{
  printf("%lx", e);
}

