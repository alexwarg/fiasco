#include "vkey.h"
#include "kernel_console.h"

namespace Vkey
{
void irq(Irq_base *i) {}
void set_echo(Echo_type) {}
void set_echo(Echo_type) {}
void clear() {}
void add_char(int) {}

int get()
{
  return Kconsole::console()->getchar(0);
}

int check_(int = -1)
{ return 0; }

}

