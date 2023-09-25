
#include "types.h"

#include <cstdlib>
#include <cstdio>
#include <construction.h>
#include "terminate.h"
#include "main.h"


extern "C"
void __main()
{
  static_construction();
  kernel_main();
  terminate(0);
}
