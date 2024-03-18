#pragma once

#include <cxx/type_traits>

//
// Regular lock-guard policy, lock on ctor, unlock/reset in dtor
//
template< typename LOCK >
struct Lock_guard_regular_policy
{
  typedef typename LOCK::Status Status;
  static Status test_and_set(LOCK *l) { return l->test_and_set(); }
  static void set(LOCK *l, Status s) { l->set(s); }
};

//
// Inverse lock-guard policy, unlock in ctor, lock in dtor
// NOTE: this is applicable only to some locks (e.g., the cpu lock)
//
template<typename LOCK>
struct Lock_guard_inverse_policy : private LOCK
{
  typedef typename LOCK::Status Status;
  static Status test_and_set(LOCK *l) { l->clear(); return LOCK::Locked; }
  static void set(LOCK *l, Status s) { l->set(s); }
};


//
// Lock_guard: a guard object using a lock such as helping_lock_t
//
template<
  typename LOCK,
  template< typename L > class POLICY = Lock_guard_regular_policy >
class Lock_guard
{
  typedef typename cxx::remove_pointer<typename cxx::remove_reference<LOCK>::type>::type Lock;
  typedef POLICY<Lock> Policy;

  Lock *_lock = nullptr;
  typename Policy::Status _state;

public:
  Lock_guard(Lock_guard &) = delete;
  Lock_guard &operator = (Lock_guard &) = delete;

  Lock_guard() = default;

  Lock_guard(Lock_guard &&l) noexcept
  : _lock(l._lock), _state(l._state)
  {
    l.release();
  }

  Lock_guard &operator = (Lock_guard &&l) noexcept
  {
    reset();
    _lock = l._lock;
    _state = l._state;
    l.release();
    return *this;
  }

  constexpr explicit Lock_guard(Lock *l) noexcept
  : _lock(l), _state(Policy::test_and_set(l))
    {}

  void lock(Lock *l) noexcept
  {
    _lock = l;
    _state = Policy::test_and_set(l);
  }

  bool check_and_lock(Lock *l) noexcept
  {
    _lock = l;
    _state = Policy::test_and_set(l);
    return _state != Lock::Invalid;
  }

  bool try_lock(Lock *l) noexcept
  {
    _state = Policy::test_and_set(l);
    switch (_state)
      {
      case Lock::Locked:
        return true;
      case Lock::Not_locked:
        _lock = l;			// Was not locked -- unlock.
        return true;
      default:
        return false; // Error case -- lock not existent
      }
  }

  void release() noexcept
  {
    _lock = nullptr;
  }

  void reset() noexcept
  {
    if (_lock)
      {
        Policy::set(_lock, _state);
        _lock = nullptr;
      }
  }

  ~Lock_guard() noexcept
  {
    if (_lock)
      Policy::set(_lock, _state);
  }


};

template< typename LOCK>
class Lock_guard_2
{
  LOCK *_l1 = nullptr;
  LOCK *_l2 = nullptr;

  typename LOCK::Status _state1, _state2;

public:
  Lock_guard_2() = default;

  Lock_guard_2(LOCK *l1, LOCK *l2) noexcept
    : _l1(l1 < l2 ? l1 : l2), _l2(l1 < l2 ? l2 : l1)
  {
    _state1 = _l1->test_and_set();
    if (_l1 == _l2)
      _l2 = nullptr;
    else
      _state2 = _l2->test_and_set();
  }

  void lock(LOCK *l1, LOCK *l2) noexcept
  {
    _l1 = l1 < l2 ? l1 : l2;
    _l2 = l1 < l2 ? l2 : l1;
    _state1 = _l1->test_and_set();
    if (_l1 == _l2)
      _l2 = nullptr;
    else 
      _state2 = _l2->test_and_set();
  }

  bool check_and_lock(LOCK *l1, LOCK *l2) noexcept
  {
    _l1 = l1 < l2 ? l1 : l2;
    _l2 = l1 < l2 ? l2 : l1;
    if ((_state1 = _l1->test_and_set()) == LOCK::Invalid)
      {
        _l1 = _l2 = nullptr;
        return false;
      }

    if (_l1 == _l2)
      _l2 = nullptr;
    else if ((_state2 = _l2->test_and_set()) == LOCK::Invalid)
      {
        _l2 = nullptr;
        return false;
      }

    return true;
  }

  ~Lock_guard_2() noexcept
  {
    if (_l2)
      _l2->set(_state2);

    if (_l1)
      _l1->set(_state1);
  }
};


template<template<typename L> class POLICY = Lock_guard_regular_policy, typename LOCK>
constexpr Lock_guard<LOCK, POLICY> lock_guard(LOCK &lock)
{ return Lock_guard<LOCK, POLICY>(&lock); }

template<template<typename L> class POLICY = Lock_guard_regular_policy, typename LOCK>
constexpr Lock_guard<LOCK, POLICY> lock_guard(LOCK *lock)
{ return Lock_guard<LOCK, POLICY>(lock); }

template<template<typename L> class POLICY = Lock_guard_regular_policy, typename LOCK>
constexpr Lock_guard<LOCK, POLICY> lock_guard_dont_lock(LOCK &)
{ return Lock_guard<LOCK, POLICY>(); }

template<template<typename L> class POLICY = Lock_guard_regular_policy, typename LOCK>
constexpr Lock_guard<LOCK, POLICY> lock_guard_dont_lock(LOCK *)
{ return Lock_guard<LOCK, POLICY>(); }


