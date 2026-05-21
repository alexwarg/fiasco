#pragma once

#include <irq_chip_generic.h>
#include <types.h>

class Gic : public Irq_chip_gen
{
public:
  static Gic *primary;

  virtual void softint_cpu(Cpu_number target, unsigned m) = 0;

  // init / pm only functions (rarely used)
  virtual void softint_bcast(unsigned m) = 0;
  virtual void softint_phys(unsigned m, Unsigned64 target) = 0;
  virtual unsigned gic_version() const = 0;

  // empty default for JDB
  virtual void irq_prio_bootcpu(unsigned, unsigned) {}
  virtual unsigned irq_prio_bootcpu(unsigned) { return 0; }
  virtual unsigned get_pmr() { return 0; }
  virtual void set_pmr(unsigned) {}
  virtual unsigned get_pending() { return 1023; }
};

