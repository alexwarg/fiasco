
#include <config.h>
#include <initcalls.h>

const char *const Config::kernel_warn_config_string = 0;

FIASCO_INIT
void
Config::init_arch()
{
  // set a smaller default for JDB trace buffers
  Config::tbuf_entries = 1024;
}

