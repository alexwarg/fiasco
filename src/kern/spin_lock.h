#pragma once

#include "cpu_lock.h"
#include "types.h"
#include "mem.h"
#include "arch_spin_lock.h"

class Spin_lock_base : protected Cpu_lock
{
public:
  enum Lock_init { Unlocked = 0 };
};

/**
 * Optimized lock-guard policy for Spin_lock that does not disable IRQs to avoid
 * the overhead for cases where it is certain that IRQs are already disabled.
 */
template< typename LOCK >
struct No_cpu_lock_policy
{
  using Status = unsigned; // unused

  static Status test_and_set(LOCK *l)
  {
    l->lock_arch();
    return 0;
  }

  static void set(LOCK *l, Status)
  { l->unlock_arch(); }
};

/*
 * The memory order guarantees provided by the spin lock are not limited to the
 * memory accesses within the critical section protected by the lock, but also
 * apply to accesses before and after the critical section.
 *
 * In other words, the following rule applies:
 * A CPU holding a lock sees all changes previously seen or made by any CPU
 * before it released that same lock.
 *
 * And also the reverse of the rule applies:
 * A CPU holding a lock does not see any changes subsequently made by any CPU
 * after it acquired that same lock.
 */
template<typename Lock_t = Small_atomic_int>
class Spin_lock : public Spin_lock_base, private Arch_spin_lock<Spin_lock<Lock_t>>
{
public:
  using Lock_type = Lock_t;
  using Arch_lock_type = Arch_spin_lock<Spin_lock<Lock_t>>;
  using Arch_lock_type::Arch_lock;
  friend class Arch_spin_lock<Spin_lock<Lock_t>>;

  template< typename LOCK >
  friend struct No_cpu_lock_policy;

  typedef Mword Status;
  Spin_lock() = default;
  explicit constexpr Spin_lock(Lock_init i) noexcept
  : _lock((i == Unlocked) ? 0 : Arch_lock) {}

  void init() noexcept
  {
    _lock = 0;
  }

  Status test() const noexcept
  {
    return (!!cpu_lock.test()) | (_lock & Arch_lock);
  }

  void lock() noexcept
  {
    //assert(!cpu_lock.test());
    cpu_lock.lock();
    this->lock_arch();
  }

  void clear() noexcept
  {
    this->unlock_arch();
    Cpu_lock::clear();
  }

  Status test_and_set() noexcept
  {
    Status s = !!cpu_lock.test();
    cpu_lock.lock();
    this->lock_arch();
    return s;
  }

  void set(Status s) noexcept
  {
    if (!(s & Arch_lock))
      this->unlock_arch();

    if (!(s & 1))
      cpu_lock.clear();
  }

protected:
  Lock_t _lock;
};


/**
 * \brief Version of a spin lock that is colocated with another value.
 */
template< typename T >
class Spin_lock_coloc : public Spin_lock<Mword>
{
public:
  Spin_lock_coloc() = default;
  explicit Spin_lock_coloc(Lock_init i) noexcept
  : Spin_lock<Mword>(i)
  {}

  T get_unused() const noexcept
  {
    return reinterpret_cast<T>(_lock & ~Arch_lock);
  }

  void set_unused(T val) noexcept
  {
    _lock = (_lock & Arch_lock) | reinterpret_cast<Mword>(val);
  }
};

