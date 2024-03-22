#pragma once

#include "ready_queue_fp.h"
#include "ready_queue_wfq.h"

#include "std_macros.h"
#include "config.h"

#include <cassert>

template<typename D>
class Sched_context_t
{
  MEMBER_OFFSET();
  friend class Jdb_list_timeouts;
  friend class Jdb_thread_list;
  friend class Ready_queue_wfq<D>;

  union Sp
  {
    L4_sched_param p;
    L4_sched_param_legacy legacy_fixed_prio;
    L4_sched_param_fixed_prio fixed_prio;
    L4_sched_param_wfq wfq;
  };

  struct Ready_list_item_concept
  {
    typedef D Item;
    static D *&next(D *e) { return e->_sc.fp._ready_next; }
    static D *&prev(D *e) { return e->_sc.fp._ready_prev; }
    static D const *next(D const *e)
    { return e->_sc.fp._ready_next; }
    static D const *prev(D const *e)
    { return e->_sc.fp._ready_prev; }
  };

public:
  enum Type { Fixed_prio, Wfq };

  typedef cxx::Sd_list<D, Ready_list_item_concept> Fp_list;

private:
  Type _t;

  struct B_sc
  {
    unsigned short _p;
    unsigned _q;
    Unsigned64 _left;

    unsigned prio() const { return _p; }
  };


  struct Fp_sc : public B_sc
  {
    D *_ready_next, *_ready_prev;
  };

  struct Wfq_sc : public Sched_context_wfq<Wfq_sc>, public B_sc
  {
    D **_ready_link;
    bool _idle;
    Unsigned64 _dl;

    unsigned _w;
    unsigned _qdw;
  };

  union Sc
  {
    Wfq_sc wfq;
    Fp_sc fp;
  };

  Sc _sc;

public:
  static Wfq_sc *wfq_elem(Sched_context_t *x) { return &x->_sc.wfq; }

  struct Ready_queue_base
  {
  public:
    Ready_queue_fp<D> fp_rq;
    Ready_queue_wfq<D> wfq_rq;
    D *current_sched() const { return _current_sched; }
    void activate(D *s)
    {
      if (!s || s->_t == Wfq)
        wfq_rq.activate(s);
      _current_sched = s;
    }

    void enqueue(D *sc, bool is_current)
    {
      if (sc->_t == Fixed_prio)
        fp_rq.enqueue(sc, is_current);
      else
        wfq_rq.enqueue(sc, is_current);
    }

    void dequeue(D *sc)
    {
      if (sc->_t == Fixed_prio)
        fp_rq.dequeue(sc);
      else
        wfq_rq.dequeue(sc);
    }

    void requeue(D *sc)
    {
      if (sc->_t == Fixed_prio)
        fp_rq.requeue(sc);
      else
        wfq_rq.requeue(sc);
    }

    void set_idle(D *sc)
    {
      sc->_t = Wfq;
      sc->_sc.wfq._p = 0;
      wfq_rq.set_idle(sc);
    }

    D *next_to_run() const
    {
      D *s = fp_rq.next_to_run();
      if (s)
        return s;

      return wfq_rq.next_to_run();
    }

    void deblock_refill(D *sc)
    {
      if (sc->_t != Wfq)
        fp_rq.deblock_refill(sc);
      else
        wfq_rq.deblock_refill(sc);
    }


  private:
    friend class Jdb_thread_list;
    D *_current_sched;
  };


  constexpr static Mword sched_classes()
  {
    return 1UL << (-L4_sched_param_fixed_prio::Class)
         | 1UL << (-L4_sched_param_wfq::Class);
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

      case L4_sched_param_wfq::Class:
        if (!_p->check_length<L4_sched_param_wfq>())
          return -L4_err::EInval;
        if (p->wfq.quantum == 0 || p->wfq.weight == 0)
          return -L4_err::EInval;
        break;

      default:
        if (!_p->is_legacy())
          return -L4_err::ERange;
        break;
      }

    return 0;
  }

  Sched_context_t()
  {
    _t = Fixed_prio;
    _sc.fp._p = Config::Default_prio;
    _sc.fp._q = Config::Default_time_slice;
    _sc.fp._left = Config::Default_time_slice;
    _sc.fp._ready_next = nullptr;
  }

  Context *context() const noexcept
  {
    return context_of(this);
  }

  unsigned prio() const noexcept
  {
    return _sc.fp._p;
  }

  bool in_ready_list() const noexcept
  {
    // this magically works for the fp list and the heap,
    // because wfq._ready_link and fp._ready_next are the
    // same memory location
    return _sc.wfq._ready_link != 0;
  }

  bool dominates(D const *sc) const noexcept
  {
    if (_t == Fixed_prio)
      return prio() > sc->prio();

    if (_sc.wfq._idle)
      return false;

    if (sc->_t == Fixed_prio)
      return false;

    return _sc.wfq._dl < sc->_sc.wfq._dl;
  }

  void replenish() noexcept
  {
    _sc.fp._left = _sc.fp._q;
    if (_t == Wfq)
      _sc.wfq._dl += _sc.wfq._qdw;
  }

  void set_left(Unsigned64 l) noexcept
  {
    _sc.fp._left = l;
  }

  Unsigned64 left() const noexcept
  {
    return _sc.fp._left;
  }

  void set(L4_sched_param const *_p)
  {
    Sp const *p = reinterpret_cast<Sp const *>(_p);
    if (_p->is_legacy())
      {
        // legacy fixed prio
        _t = Fixed_prio;
        _sc.fp._p = p->legacy_fixed_prio.prio;
        if (p->legacy_fixed_prio.prio > 255)
          _sc.fp._p = 255;

        _sc.fp._q = p->legacy_fixed_prio.quantum;
        if (p->legacy_fixed_prio.quantum == 0)
          _sc.fp._q = Config::Default_time_slice;
        return;
      }

    switch (p->p.sched_class)
      {
      case L4_sched_param_fixed_prio::Class:
        _t = Fixed_prio;

        _sc.fp._p = p->fixed_prio.prio;
        if (p->fixed_prio.prio > 255)
          _sc.fp._p = 255;

        _sc.fp._q = p->fixed_prio.quantum;
        if (p->fixed_prio.quantum == 0)
          _sc.fp._q = Config::Default_time_slice;

        break;
      case L4_sched_param_wfq::Class:
        _t = Wfq;
        _sc.wfq._p = 0;
        _sc.wfq._q = p->wfq.quantum;
        _sc.wfq._w = p->wfq.weight;
        _sc.wfq._qdw =  p->wfq.quantum / p->wfq.weight;
        break;

      default:
        assert(false && "Missing check_param()?");
        break;
      };
  }

};

