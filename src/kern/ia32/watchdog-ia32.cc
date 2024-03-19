
#include "watchdog.h"

#include "types.h"
#include "initcalls.h"

#include <cstdio>
#include <cstdlib>

#include "apic.h"
#include "config.h"
#include "cpu.h"
#include "panic.h"
#include "perf_cnt.h"
#include "static_init.h"

namespace {

struct Watchdog_flags
{
  unsigned active:1;
  unsigned user_active:1;
  unsigned no_user_control:1;
};

static Watchdog_flags flags;


#define WATCHDOG_TIMEOUT_S	2

static void do_nothing()
{}

static void perf_enable()
{
  if (flags.active && flags.user_active)
    {
      Perf_cnt::touch_watchdog();
      Perf_cnt::start_watchdog();
      Apic::set_perf_nmi();
    }
}

static void perf_disable()
{
  if (flags.active)
    Perf_cnt::stop_watchdog();
}

static void perf_touch()
{
  if (flags.active && flags.user_active && flags.no_user_control)
    Perf_cnt::touch_watchdog();
}

}

namespace Watchdog
{
  Fn touch = do_nothing;
  Fn enable = do_nothing;
  Fn disable = do_nothing;

  static void FIASCO_INIT
  init()
  {
    if (!Config::watchdog)
      return;

    printf("Watchdog: LAPIC=%s, PCINT=%s, PC=%s\n",
           Apic::is_present() ? "yes" : "no",
           Apic::have_pcint() ? "yes" : "no",
           Perf_cnt::have_watchdog() ? "yes" : "no");

    if (   !Apic::is_present()
        || !Apic::have_pcint()
        || !Perf_cnt::have_watchdog())
      panic("Cannot initialize watchdog (no/bad Local APIC)");

    // attach performance counter interrupt to NMI
    Apic::set_perf_nmi();

    // start counter
    Perf_cnt::setup_watchdog(WATCHDOG_TIMEOUT_S);

    touch   = perf_touch;
    enable  = perf_enable;
    disable = perf_disable;

    flags.active = 1;
    flags.user_active = 1;
    flags.no_user_control = 1;

    printf("Watchdog initialized\n");
  }
}

STATIC_INITIALIZE_P(Watchdog, WATCHDOG_INIT);

