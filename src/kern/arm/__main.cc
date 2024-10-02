
#include "types.h"

#include <cstdlib>
#include <cstdio>
#include <construction.h>
#include "main.h"

extern "C" [[noreturn]] void __main();
[[noreturn]] void kernel_main(void);

extern "C" [[noreturn]]
void __main()
{
  static_construction();
  kernel_main();
}
