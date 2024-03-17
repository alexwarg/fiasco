#include "kobject_iface.h"
#include "panic.h"

Kobject_iface::Factory_func *Kobject_iface::factory[Max_factory_index + 1];

void
Kobject_iface::set_factory(long label, Factory_func *f)
{
  if (label > 0 || -label > Max_factory_index)
    panic("error: registering factory for invalid protocol/label: %ld\n",
          label);

  if (factory[-label])
    panic("error: factory for protocol/label %ld already registered: %p\n",
          label, factory[-label]);

  factory[-label] = f;
}

