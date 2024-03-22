#pragma once

#include "member_offs.h"
#include "types.h"
#include "globals.h"
#include "ready_queue_wfq.h"
#include "std_macros.h"
#include "config.h"

#include <cassert>

template<typename D>
class Sched_context_t : public Sched_context_wfq<D>
{
  MEMBER_OFFSET();
  friend class Jdb_list_timeouts;
  friend class Jdb_thread_list;

  template<typename T>
  friend class Jdb_thread_list_policy;
  friend class Sched_context_wfq<D>;

  union Sp
  {
    L4_sched_param p;
    L4_sched_param_wfq wfq;
  };

public:
  typedef D Wfq_sc;
  typedef Ready_queue_wfq<D> Ready_queue_base;

  constexpr static Mword sched_classes()
  {
    return 1UL << (-L4_sched_param_wfq::Class);
  }

  static int check_param(L4_sched_param const *_p)
  {
    Sp const *p = reinterpret_cast<Sp const *>(_p);
    if (p->p.sched_class != L4_sched_param_wfq::Class)
      return -L4_err::ERange;

    if (!_p->check_length<L4_sched_param_wfq>())
      return -L4_err::EInval;

    if (p->wfq.quantum == 0 || p->wfq.weight == 0)
      return -L4_err::EInval;

    return 0;
  }


  Sched_context_t() = default;

  Context *context() const noexcept
  {
    return context_of(this);
  }

  static unsigned prio() noexcept
  {
    return 0;
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
    set_left(_q);
    _dl += _qdw;
  }

  bool in_ready_list() const noexcept
  {
    return _ready_link != 0;
  }

  bool dominates(D const *sc) const noexcept
  {
#if 0
    if (_idle)
      LOG_MSG_3VAL(current(), "idl", (Mword)sc, _dl, sc->_dl);
#endif
    return !_idle && _dl < sc->_dl;
  }

  Context *owner() const noexcept
  {
    return context();
  }

  void set(L4_sched_param const *_p)
  {
    Sp const *p = reinterpret_cast<Sp const *>(_p);
    _dl = 0;
    _q = p->wfq.quantum;
    _w = p->wfq.weight;
    _qdw =  p->wfq.quantum / p->wfq.weight;
  }


private:
  static D *wfq_elem(D *x) { return x; }

  D **_ready_link = nullptr;
  bool _idle = false;
  Unsigned64 _dl = 0;
  Unsigned64 _left = Config::Default_time_slice;

  unsigned _q = Config::Default_time_slice;
  unsigned _w = 1;
  unsigned _qdw = Config::Default_time_slice / 1; // (_q / _w)

  friend class Ready_queue_wfq<D>;
};

