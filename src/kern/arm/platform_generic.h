
#include <platform_iface.h>
#include <irq_chip.h>
#include <irq_mgr.h>
#include <panic.h>

class Irq_chip;

class Platform_base : public Platform_if_base
{
public:
  void init_irqs_ap(Cpu_number cpu, bool resume) override
  {
    static_cast<Irq_mgr_dyn *>(Irq_mgr::mgr)->init_ap(cpu, resume);
  }
};

