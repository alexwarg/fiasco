#pragma once

#include "types.h"
#include "config.h"
#include "config_tcbsize.h"
#include "fiasco_defs.h"
#include "processor.h"

class Context;

class Context_base
{
public:
  enum
  {
    Size = THREAD_BLOCK_SIZE
  };

  // This virtual dtor enforces that Context / Thread / Context_base
  // all start at offset 0
  virtual ~Context_base() = 0;

  void set_current_cpu(Cpu_number cpu)
  {
    _cpu = cpu;
  }

  Cpu_number get_current_cpu() const
  {
    return _cpu;
  }

protected:
  Mword _state;

private:
  friend Cpu_number current_cpu();
  Cpu_number _cpu;
};


inline Context_base::~Context_base() {}

inline
Context *context_of(const void *ptr)
{
  return reinterpret_cast<Context *>
    (reinterpret_cast<unsigned long>(ptr) & ~(Context_base::Size - 1));
}

inline
Context *current()
{
  return context_of((void *)Proc::stack_pointer());
}

inline
Cpu_number FIASCO_PURE current_cpu()
{
  return reinterpret_cast<Context_base *>(current())->_cpu;
}

