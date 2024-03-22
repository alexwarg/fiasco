
/*
 * Timeslice infrastructure
 */

INTERFACE [sched_fixed_prio]:

#include <cxx/dlist>
#include "member_offs.h"
#include "types.h"
#include "globals.h"
#include "ready_queue_fp.h"
#include "std_macros.h"
#include "config.h"

#include <cassert>

template<typename D>
class Sched_context_t : public cxx::D_list_item
{
  MEMBER_OFFSET();
  friend class Jdb_list_timeouts;
  friend class Jdb_thread_list;
  friend class Sched_ctxts_test;
  friend class Scheduler_test;

  template<typename T>
  friend struct Jdb_thread_list_policy;

  union Sp
  {
    L4_sched_param p;
    L4_sched_param_legacy legacy_fixed_prio;
    L4_sched_param_fixed_prio fixed_prio;
  };

public:
  typedef cxx::Sd_list<D> Fp_list;

  class Ready_queue_base : public Ready_queue_fp<D>
  {
  public:
    void activate(D *s)
    {
      _current_sched = s;
    }

    D *current_sched() const
    {
      return _current_sched;
    }

  private:
    D *_current_sched;
  };

  constexpr static Mword sched_classes()
  {
    return 1UL << (-L4_sched_param_fixed_prio::Class);
  }

  static int check_param(L4_sched_param const *_p)
  {
    Sp const *p = reinterpret_cast<Sp const *>(_p);
    switch (p->p.sched_class)
      {
      case L4_sched_param_fixed_prio::Class:
        if (!_p->check_length<L4_sched_param_fixed_prio>())
          return -L4_err::EInval;
        break;

      default:
        if (!_p->is_legacy())
          return -L4_err::ERange;
        break;
      }

    return 0;
  }

  Sched_context_t() = default;

  Context *context() const noexcept
  {
    return context_of(this);
  }

  unsigned short prio() const noexcept
  {
    return _prio;
  }

  Unsigned64 left() const noexcept
  {
    return _left;
  }

  void set_left(Unsigned64 left) noexcept
  {
    _left = left;
  }

  void replenish() noexcept
  {
    set_left(_quantum);
  }

  bool in_ready_list() const noexcept
  {
    return Fp_list::in_list(static_cast<D const *>(this));
  }

  bool dominates(D const *sc) const noexcept
  {
    return prio() > sc->prio();
  }

  void set(L4_sched_param const *_p)
  {
    Sp const *p = reinterpret_cast<Sp const *>(_p);
    if (_p->is_legacy())
      {
        // legacy fixed prio
        _prio = p->legacy_fixed_prio.prio;
        if (p->legacy_fixed_prio.prio > 255)
          _prio = 255;

        _quantum = p->legacy_fixed_prio.quantum;
        if (p->legacy_fixed_prio.quantum == 0)
          _quantum = Config::Default_time_slice;
        return;
      }

    switch (p->p.sched_class)
      {
      case L4_sched_param_fixed_prio::Class:
        _prio = p->fixed_prio.prio;
        if (p->fixed_prio.prio > 255)
          _prio = 255;

        _quantum = p->fixed_prio.quantum;
        if (p->fixed_prio.quantum == 0)
          _quantum = Config::Default_time_slice;
        break;

      default:
        assert(false && "Missing check_param()?");
        break;
      }
  }

private:
  friend class Ready_queue_fp<D>;

  unsigned short _prio = Config::Default_prio;
  Unsigned64 _quantum = Config::Default_time_slice;
  Unsigned64 _left = Config::Default_time_slice;
};

