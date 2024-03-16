#include "rcupdate.h"
#include "cpu.h"
#include "cpu_lock.h"
#include "globals.h"
#include "lock_guard.h"
#include "mem.h"
#include "static_init.h"
#include "timeout.h"
#include "logdefs.h"


class Rcu_timeout : public Timeout
{
public:
  /**
   * Timeout expiration callback function
   * @return true if reschedule is necessary, false otherwise
   */
  bool expired() override
  { return Rcu::process_callbacks(); }
};

Rcu_glbl Rcu::_rcu INIT_PRIORITY(EARLY_INIT_PRIO);
DEFINE_PER_CPU Per_cpu<Rcu_data> Rcu::_rcu_data(Per_cpu_data::Cpu_num);
DEFINE_PER_CPU static Per_cpu<Rcu_timeout> _rcu_timeout;

Rcu_data::~Rcu_data()
{
  if (current_cpu() == _cpu)
    return;

  Rcu_data *current_rdp = &Rcu::_rcu_data.current();
  Rcu_glbl *rgp = Rcu::rcu();

    {
      auto guard = lock_guard(rgp->_lock);
      if (rgp->_current != rgp->_completed)
        rgp->cpu_quiet(_cpu);
    }

  current_rdp->move_batch(_c);
  current_rdp->move_batch(_n);
  current_rdp->move_batch(_d);
}

bool
Rcu_data::process_callbacks(Rcu_glbl *rgp)
{
  LOG_TRACE("Rcu callbacks", "rcu", ::current(), Rcu::Log_rcu,
      l->cpu = _cpu;
      l->item = 0;
      l->event = Rcu::Rcu_process);

  if (!_c.empty() && rgp->_completed >= _batch)
    _d.append(_c);

  if (!_n.empty() && _c.empty())
    {
        {
          auto guard = lock_guard(cpu_lock);
          _c = cxx::move(_n);
        }

      // start the next batch of callbacks

      _batch = rgp->_current + 1;
      Mem::mp_rmb();

      if (!rgp->_next_pending)
        {
          // start the batch and schedule start if it's a new batch
          auto guard = lock_guard(rgp->_lock);
          rgp->_next_pending = true;
          rgp->start_batch();
        }
    }

  check_quiescent_state(rgp);
  if (!_d.empty())
    return do_batch();

  return false;
}

void
Rcu::schedule_callbacks(Cpu_number cpu, Unsigned64 clock)
{
  Timeout *t = &_rcu_timeout.cpu(cpu);
  if (!t->is_set())
    t->set(clock, cpu);
}

