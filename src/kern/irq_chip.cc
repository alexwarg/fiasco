#include "irq_chip.h"
#include "static_init.h"

Irq_chip_soft Irq_chip_soft::sw_chip INIT_PRIORITY(EARLY_INIT_PRIO);
Irq_base *(*Irq_base::dcast)(Kobject_iface *);

