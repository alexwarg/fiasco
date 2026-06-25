#include "vkey.h"
#include "kernel_console.h"

namespace Vkey
{
void irq(Irq_base *i) {}
void set_echo(Echo_type) {}
void clear() {}
void irq(Irq_base *const *) {}

int get()
{
  return Kconsole::console()->getchar(0);
}

}

