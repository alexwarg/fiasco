#pragma once

#include "irq.h"
#include "irq_mgr.h"
#include "ram_quota.h"
#include "icu_helper.h"

#include <cstdio>

class Irq_chip;

class Icu : public Icu_h<Icu>
{
  friend class Icu_h<Icu>;

public:
  void operator delete (void *)
  {
    printf("WARNING: tried to delete kernel ICU object.\n"
           "         The system is now useless\n");
  }

  Irq_base *icu_get_irq(unsigned irqnum) const
  {
    return Irq_mgr::mgr->irq(irqnum);
  }

  Irq_mgr::Irq icu_get_chip(Mword pin) const
  {
    return Irq_mgr::mgr->chip(pin);
  }

  int icu_bind_irq(unsigned irqnum, Irq_base *irq)
  {
    if (Irq_mgr::mgr->alloc(irq, irqnum))
      return 0;

    return -L4_err::EInval;
  }

  L4_msg_tag op_icu_get_info(Mword *features, Mword *num_irqs, Mword *num_msis)
  {
    *num_irqs = Irq_mgr::mgr->nr_irqs();
    *num_msis = Irq_mgr::mgr->nr_msis();
    *features = *num_msis ? (unsigned)Msi_bit : 0;
    return L4_msg_tag(0);
  }

  L4_msg_tag op_icu_msi_info(Mword msi, Unsigned64 source, Irq_mgr::Msi_info *out)
  {
    return commit_result(Irq_mgr::mgr->msg(msi, source, out));
  }


  Icu()
  {
    initial_kobjects.register_obj(this, Initial_kobjects::Icu);
  }

};

