#include "banner.h"

#include <cstdio>
#include "config.h"
#include "initcalls.h"

extern char _initkip_start[];

namespace Banner {

FIASCO_INIT void
init()
{
  printf("\n%s\n", _initkip_start);
  if (Config::kernel_warn_config_string && *Config::kernel_warn_config_string)
    printf("\033[31mPerformance-critical config option(s) detected:\n"
	   "%s\033[m", Config::kernel_warn_config_string);
  putchar('\n');
}

}
