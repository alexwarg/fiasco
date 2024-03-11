INTERFACE:

#include <cstddef>
#include <types.h>
#include <cxx/atomic>

class Ram_quota
{
public:
  static Ram_quota *root;
  virtual ~Ram_quota() = 0;

private:
  Ram_quota *_parent;
  cxx::atomic<Mword> _current;

  enum : Mword
  { Invalid = 1UL << ((sizeof(Mword) * 8) - 1) };

  Mword _max;
};


IMPLEMENTATION:

Ram_quota *Ram_quota::root;

IMPLEMENT inline Ram_quota::~Ram_quota() {}

PUBLIC static inline
bool
Ram_quota::check_max(Mword max)
{
  // we actually allow one byte less to always allow
  // 1 byte spare for take_and_invalidate() / put() handling
  return (max != 0) && (max < (Invalid - 1));
}

PUBLIC inline NEEDS[<cstddef>]
void *
Ram_quota::operator new (size_t, void *b) throw()
{ return b; }

PUBLIC
Ram_quota::Ram_quota()
  : _parent(0), _current(0), _max(0)
{
  root = this;
}

PUBLIC
Ram_quota::Ram_quota(Ram_quota *p, Mword max)
  : _parent(p), _current(0), _max(max)
{}


PUBLIC inline
Mword
Ram_quota::current() const
{ return _current.load(cxx::memory_order_relaxed) & ~Invalid; }

PUBLIC
bool
Ram_quota::alloc(Mword bytes)
{
  // prevent overflow
  if (bytes >= Invalid)
    return false;

  if (unlimited())
    return true;

  Mword o = _current.load(cxx::memory_order_relaxed);
  for (;;)
    {
      if (o & Invalid)
        return false;

      Mword n = o + bytes;
      if (n > _max)
        return false;

      if (_current.compare_exchange_weak(o, n))
        return true;
    }
}

PUBLIC inline
bool
Ram_quota::alloc(Bytes size)
{
  return alloc(cxx::int_value<Bytes>(size));
}

PRIVATE inline NEEDS[<cxx/atomic>]
bool
Ram_quota::_free_bytes(Mword bytes)
{
  if (unlimited())
    return false;

  Mword r = _current -= bytes;

  return r == Invalid;
}

PUBLIC inline NEEDS[Ram_quota::_free_bytes]
void
Ram_quota::free(Mword bytes)
{
  if (_free_bytes(bytes))
    delete this;
}

PUBLIC inline NEEDS[Ram_quota::_free_bytes]
void
Ram_quota::free(Bytes size)
{
  free(cxx::int_value<Bytes>(size));
}

/**
 * Allocate one byte to prevent immediate deletion, and
 * mark the object as invalid.
 */
PROTECTED inline NEEDS[<cxx/atomic>]
void
Ram_quota::take_and_invalidate()
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

PUBLIC inline NEEDS[Ram_quota::_free_bytes]
bool
Ram_quota::put()
{
  return _free_bytes(1);
}

PUBLIC inline
Ram_quota*
Ram_quota::parent() const
{ return _parent; }

PUBLIC inline
Mword
Ram_quota::limit() const
{ return _max; }

PUBLIC inline
bool
Ram_quota::unlimited() const
{ return _max == 0; }

