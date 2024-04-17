#include "types.h"
#include "initcalls.h"

#include "boot_info.h"
#include "initcalls.h"

#include <cstdlib>
#include <cstdio>
#include <construction.h>
#include "main.h"

extern "C" FIASCO_FASTCALL FIASCO_INIT
void __main(unsigned checksum_ro);

extern "C" FIASCO_FASTCALL FIASCO_INIT
void
__main(unsigned checksum_ro)
{
  /* set global to be used in the constructors */
  Boot_info::set_checksum_ro(checksum_ro);

  atexit(&static_destruction);
  static_construction();

  kernel_main();
  exit(0);
}
