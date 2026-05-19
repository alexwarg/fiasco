
#include <cstring>
#include <koptions.h>
#include <perf_cnt.h>
#include <static_init.h>

static void FIASCO_INIT_CPU
loadcnt_init()
{
  if (Koptions::o()->opt(Koptions::F_loadcnt))
    Perf_cnt::setup_loadcnt();
}

STATIC_INITIALIZER_P(loadcnt_init, WATCHDOG_INIT);

