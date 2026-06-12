
#include "watchdog.h"

#include "initcalls.h"
#include "mmio_register_block.h"
#include "kmem_mmio.h"
#include <mem_layout.h>
#include "l4_types.h"
#include "io.h"
#include "static_init.h"

#include <cstdio>


namespace Watchdog
{

class Watchdog : private Mmio_register_block
{
private:
  enum {
    WTCON = 0x0,
    WTDAT = 0x4,
    WTCNT = 0x8,

    WTCON_RST_EN    = 1 << 0,
    WTCON_EN        = 1 << 5,
    WTCON_PRESCALER = (0x10 << 8),

    Reset_timeout_val = 299999 * 1000,
  };

public:
  static Static_object<Watchdog> wdog;

  Watchdog(void *virt) : Mmio_register_block(virt) {}

  static void enable()
  {
    wdog->write(wdog->read<Mword>(WTCON) | WTCON_EN, WTCON);
  }

  static void disable()
  {
    wdog->write(wdog->read<Mword>(WTCON) & ~WTCON_EN, WTCON);
  }

  static void touch()
  {
    wdog->write<Mword>(Reset_timeout_val, WTCNT);
  }

  static void setup(Mword val)
  {
    wdog->write<Mword>(0, WTCON); // disable
    wdog->write<Mword>(val, WTDAT); // set initial values
    wdog->write<Mword>(val, WTCNT);

    wdog->write<Mword>(WTCON_RST_EN | WTCON_PRESCALER, WTCON);
  }

};

Static_object<Watchdog> Watchdog::wdog;

Fn touch = Watchdog::touch;
Fn enable = Watchdog::enable;
Fn disbale = Watchdog::disable;

static FIASCO_INIT void init()
{
  Watchdog::wdog.construct(Kmem_mmio::map(Mem_layout::Watchdog_phys_base, 0x10));
  if (1)
    {
      Watchgod::wdog->setup(Reset_timeout_val);
      printf("Watchdog initialized\n");
    }
  else
    printf("Watchdog NOT running\n");
}

} // namespace Watchdog

STATIC_INITIALIZE_P(Watchdog, WATCHDOG_INIT);
