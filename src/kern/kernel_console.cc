#include "kernel_console.h"

#include "config.h"
#include "console.h"
#include "mux_console.h"
#include "processor.h"
#include <system_clock.h>

#include <cstring>
#include <cstdio>

Static_object<Kconsole> Kconsole::_c;

void
Kconsole::init()
{ _c.construct(); }

#ifdef CONFIG_INPUT
int Kconsole::getchar(bool blocking)
{
  if (check_input_ignore())
    return -1;

  if (!blocking)
    return Mux_console::getchar(false);

  while (1)
    {
      int c;
      if ((c = Mux_console::getchar(false)) != -1)
        return c;

      if (Config::getchar_does_hlt_works_ok // wakeup timer is enabled
          && Proc::interrupts())            // doesn't work without ints
        Proc::halt();
      else
        Proc::pause();
    }
}

int
Kconsole::check_input_ignore()
{
  if (_ignore_input_until)
    {
      if (System_clock::aux_clock() > _ignore_input_until)
        _ignore_input_until = 0;
      else
        {
          static unsigned releasepos;
          const char *releasestring = "input";

          // when we ignore input we read everything which comes in even if
          // input on a console is disabled
          int r;
          while ((r = Mux_console::getchar(false)) != -1)
            {
              if (releasestring[releasepos] == r)
                {
                  releasepos++;
                  if (releasepos == strlen(releasestring))
                    {
                      printf("\nJDB: Input activated.\n");
                      _ignore_input_until = 0;
                      releasepos = 0;
                      return 0;
                    }
                }
              else
                releasepos = 0;
            }
        }
    }

  return 0;
}
#endif

