#include <libc_backend.h>

#include <cxx/atomic>
#include <cpu_lock.h>
#include "mem.h"
#include "processor.h"

static cxx::atomic<Mword> __libc_backend_printf_spinlock{~0UL};

void __libc_backend_printf_local_force_unlock()
{
  Mword pid = cxx::int_value<Cpu_phys_id>(Proc::cpu_id());
  if (__libc_backend_printf_spinlock.load(cxx::memory_order_relaxed) == pid)
    __libc_backend_printf_spinlock = ~0UL;
}

unsigned int __libc_backend_printf_lock()
{
  unsigned int r = cpu_lock.test() ? 1 : 0;
  cpu_lock.lock();

  Mword pid = cxx::int_value<Cpu_phys_id>(Proc::cpu_id());
  Mword x = __libc_backend_printf_spinlock.load(cxx::memory_order_relaxed);

  // support nesting
  if (x == pid)
    return r | 2;

  for (;;)
    {
      if (x != ~0UL)
        {
          Proc::pause();
          x = __libc_backend_printf_spinlock.load(cxx::memory_order_relaxed);
          continue;
        }

      if (__libc_backend_printf_spinlock.compare_exchange_weak(x, pid))
        return r;
    }
}

void __libc_backend_printf_unlock(unsigned int state)
{
  if (!(state & 2))
    __libc_backend_printf_spinlock = ~0UL;

  if (!(state & 1))
    cpu_lock.clear();
}
