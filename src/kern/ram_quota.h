#pragma once

#include <cstddef>
#include <types.h>
#include <cxx/atomic>

class Ram_quota
{
  enum : Mword
  { Invalid = 1UL << ((sizeof(Mword) * 8) - 1) };

public:
  static Ram_quota *root;

  static bool check_max(Mword max)
  {
    // we actually allow one byte less to always allow
    // 1 byte spare for take_and_invalidate() / put() handling
    return (max != 0) && (max < (Invalid - 1));
  }
  void *operator new (size_t, void *b) noexcept
  { return b; }

  virtual ~Ram_quota() = default;

  Ram_quota(Ram_quota *p, Mword max) noexcept
  : _parent(p), _current(0), _max(max)
  {}

  Mword current() const noexcept
  { return _current.load(cxx::memory_order_relaxed) & ~Invalid; }

  bool alloc(Mword bytes);

  bool alloc(Bytes size)
  {
    return alloc(cxx::int_value<Bytes>(size));
  }

  void free(Mword bytes)
  {
    if (_free_bytes(bytes))
      delete this;
  }

  void free(Bytes size)
  {
    free(cxx::int_value<Bytes>(size));
  }

  bool put()
  {
    return _free_bytes(1);
  }

  Ram_quota *parent() const
  { return _parent; }

  Mword limit() const
  { return _max; }

  bool unlimited() const
  { return _max == 0; }

private:
  Ram_quota *_parent = nullptr;
  cxx::atomic<Mword> _current{0};
  Mword _max = 0;

  bool _free_bytes(Mword bytes)
  {
    if (unlimited())
      return false;

    Mword r = _current -= bytes;

    return r == Invalid;
  }

protected:
  /// protected root quota ctor with 'bool' arg
  explicit Ram_quota(bool) noexcept
  {
    root = this;
  }

  /**
   * Allocate one byte to prevent immediate deletion, and
   * mark the object as invalid.
   */
  void take_and_invalidate()
  {
    if (unlimited())
      return;

    Mword o = _current.load(cxx::memory_order_relaxed);
    for (;;)
      {
        Mword n = (o + 1) | Invalid;

        if (_current.compare_exchange_weak(o, n))
          return;
      }
  }

};

