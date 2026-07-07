#pragma once

#include <types.h>
#include <l4_types.h>
#include <processor.h>
#include <cxx/atomic>

namespace Ctxt
{
  class Migration
  {
  public:
    Migration() = default;

    void set_done()
    { _done = true; }

    void wait()
    {
      // FIXME: use monitor & mwait or wfe & sev if available
      while (!_done)
        Proc::pause();
    }

    void wait_locked()
    {
      cpu_lock.clear();
      wait();
      cpu_lock.lock();
    }

    // -- data below --
    Cpu_number cpu;
    L4_sched_param const *sp;

  private:
    cxx::atomic<bool> _done{false};
  };

  class Migration_ptr
  {
  public:
    bool pending() const
    { return _p.load(cxx::memory_order_relaxed); }

    Migration *operator = (Migration *m) noexcept
    {
      _p = m;
      return m;
    }

    Migration *exchange(Migration *m) noexcept
    {
      // XXX: should add exchange to cxx atomic and use it...
      Migration *old = _p;
      while (!_p.compare_exchange_weak(old, m))
        ;
      return old;
    }

    Migration *get_and_clear()
    {
      Migration *m = _p;
      if (!m)
        return nullptr;

      if (_p.compare_exchange_strong(m, nullptr))
        return m;

      return nullptr;
    }

  private:
    cxx::atomic<Migration *> _p{nullptr};
  };

}

