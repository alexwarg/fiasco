#pragma once

#include <cxx/atomic>

#include "kobject.h"
#include "irq_chip.h"
#include "prio_list.h"

class Space;

class Send_endpoint : public cxx::Dyn_castable<Send_endpoint, Kobject>
{
public:
  virtual Locked_prio_list *rcv_queue() = 0;
  virtual Space *home_space() const = 0;
  virtual void inc_ref() = 0;
  virtual void release() = 0;

  bool register_delete_irq(Irq_base *irq);
  void unregister_delete_irq();

  void remove_delete_irq(Irq_base *irq)
  {
    _del_observer.compare_exchange_strong(irq, (Irq_base *)nullptr);
  }

  void sender_deleted(Mword /*id*/)
  {
    auto g = lock_guard(cpu_lock);
    if (auto irq = _del_observer.load(cxx::memory_order_relaxed))
      irq->hit(0);
  }

private:
  cxx::atomic<Irq_base *> _del_observer{nullptr};
};
