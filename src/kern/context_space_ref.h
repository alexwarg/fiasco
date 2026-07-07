#pragma once

#include <spin_lock.h>
#include <types.h>

class Space;

class Context_space_ref
{
public:
  typedef Spin_lock_coloc<Space *> Space_n_lock;

private:
  Space_n_lock _s{Space_n_lock::Unlocked};
  Address _v = 0;

public:
  Space *space() const { return _s.get_unused(); }
  Space_n_lock *lock() { return &_s; }
  Address user_mode() const { return _v & 1; }
  Space *vcpu_user() const { return reinterpret_cast<Space*>(_v & ~3); }
  Space *vcpu_aware() const { return user_mode() ? vcpu_user() : space(); }

  void space(Space *s) { _s.set_unused(s); }
  void vcpu_user(Space *s) { _v = reinterpret_cast<Address>(s); }
  void user_mode(bool enable)
  {
    if (enable)
      _v |= Address{1};
    else
      _v &= Address{~1UL};
  }
};


