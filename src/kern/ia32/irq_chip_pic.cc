#include <irq_chip_pic.h>

#include <cassert>

#include <boot_alloc.h>
#include <cpu_lock.h>
#include <globalconfig.h>
#include <globals.h>
#include <irq_mgr.h>
#include <koptions.h>
#include <pic.h>

bool
Irq_chip_ia32_pic::alloc(Irq_base *irq, Mword irqn, bool init)
{
  // no mor than 16 IRQs
  if (irqn > 0xf)
    return false;

  // PIC uses 16 vectors from Base_vector statically
  unsigned vector = Base_vector + irqn;
  return valloc<Irq_chip_ia32_pic>(irq, irqn, vector, init);
}

void
Irq_chip_ia32_pic::unbind(Irq_base *irq)
{
  extern char entry_int_pic_ignore[];
  mask(irq->pin());
  vfree(irq, &entry_int_pic_ignore);
  Irq_chip::unbind(irq);
}

Irq_mgr::Irq
Irq_chip_ia32_pic::chip(Mword irq) const
{
  if (irq < 16)
    return Irq(const_cast<Irq_chip_ia32_pic*>(this), irq);

  return Irq();
}

unsigned
Irq_chip_ia32_pic::nr_irqs() const
{ return 16; }

unsigned
Irq_chip_ia32_pic::nr_msis() const
{ return 0; }

Irq_chip_ia32_pic::Irq_chip_ia32_pic()
: Irq_chip_i8259<Io>(0x20, 0xa0), Irq_chip_ia32(16)
{
  Irq_mgr::mgr = this;
  bool sfn = !Koptions::o()->opt(Koptions::F_nosfn);
  init(Base_vector, sfn,
       Config::Pic_prio_modify
       && (int)Config::Scheduler_mode == Config::SCHED_RTC);

  reserve(2); // reserve cascade irq
  reserve(7); // reserve spurious vect
}
