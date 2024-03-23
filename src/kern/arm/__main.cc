
#include "types.h"

#include <cstdlib>
#include <cstdio>
#include <construction.h>
#include "terminate.h"
#include "main.h"


extern "C"
void __main()
{
  atexit(&static_destruction);
  static_construction();
  kernel_main();
  terminate(0);
}
