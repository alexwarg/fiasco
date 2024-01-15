#pragma once

#include <types.h>
#include <member_offs.h>
#include <mem_space.h>

class Context;

template<typename T, typename CTXT = Context>
class Ku_mem_ptr
{
  MEMBER_OFFSET();

private:
  User_ptr<T> _u;
  T *_k;

  CTXT *context()
  { return static_cast<CTXT *>(context_of(this)); }

  CTXT const *context() const
  { return static_cast<CTXT const *>(context_of(this)); }

public:
  Ku_mem_ptr() : _u(0), _k(0) {}
  Ku_mem_ptr(User_ptr<T> const &u, T *k) : _u(u), _k(k) {}

  void set(User_ptr<T> const &u, T *k)
  { _u = u; _k = k; }

  T *access(bool is_current = false) const
  {
    // assert (!is_current || current() == context());
    if (is_current
        && (int)Config::Access_user_mem == Config::Access_user_mem_direct)
      return _u.get();

    Cpu_number const cpu = current_cpu();
    if ((int)Config::Access_user_mem == Config::Must_access_user_mem_direct
        && cpu == context()->home_cpu()
        && Mem_space::current_mem_space(cpu) == context()->space())
      return _u.get();
    return _k;
  }

  User_ptr<T> usr() const { return _u; }
  T* kern() const { return _k; }
};

