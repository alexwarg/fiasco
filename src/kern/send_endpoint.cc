#include "send_endpoint.h"
#include "cpu_lock.h"
#include "lock_guard.h"

namespace {

class Del_irq_chip : public Irq_chip_soft
{
public:
  static Del_irq_chip chip;

  static Send_endpoint *endpoint(Mword pin)
  { return reinterpret_cast<Send_endpoint *>(pin); }

  void unbind(Irq_base *irq) override
  {
    endpoint(irq->pin())->remove_delete_irq(irq);
    Irq_chip_soft::unbind(irq);
  }
};

Del_irq_chip Del_irq_chip::chip;

} // namespace

bool
Send_endpoint::register_delete_irq(Irq_base *irq)
{
  if (_del_observer.load(cxx::memory_order_relaxed))
    return false;

  auto g = lock_guard(irq->irq_lock());
  irq->unbind();
  Del_irq_chip::chip.bind(irq, reinterpret_cast<Mword>(this));
  Irq_base *none = nullptr;
  if (_del_observer.compare_exchange_strong(none, irq))
    return true;

  irq->unbind();
  return false;
}

void
Send_endpoint::unregister_delete_irq()
{
  auto irq = _del_observer.load();

  do
    {
      if (!irq)
        break;

      auto g = lock_guard(irq->irq_lock());
      irq->unbind();
    }
  while (!_del_observer.compare_exchange_weak(irq, nullptr));
}
