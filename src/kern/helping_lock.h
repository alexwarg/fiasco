#pragma once

#include "lock_guard.h"
#include "switch_lock.h"
#include "globals.h"

#ifdef NO_INSTRUMENT
#undef NO_INSTRUMENT
#endif
#define NO_INSTRUMENT __attribute__((no_instrument_function))

/**
 * A wrapper for Switch_lock that works even when the threading system
 * has not been intialized yet.
 *
 * This wrapper is necessary because most lock-protected objects are
 * initialized before the threading system has been fired up.
 */
class Helping_lock : private Switch_lock
{

public:
  using Switch_lock::Status;
  using Switch_lock::Not_locked;
  using Switch_lock::Locked;
  using Switch_lock::Invalid;

  using Switch_lock::invalidate;
  using Switch_lock::valid;
  using Switch_lock::wait_free;

  static bool threading_system_active;

  /** Constructor. */
  Helping_lock()
  {
    Switch_lock::initialize();
  }

  Status NO_INSTRUMENT test_and_set()
  {
    if (! threading_system_active) // still initializing?
      return Not_locked;

    return Switch_lock::test_and_set();
  }

  Status NO_INSTRUMENT lock()
  {
    return test_and_set();
  }

  Status NO_INSTRUMENT test()
  {
    if (EXPECT_FALSE( ! threading_system_active) ) // still initializing?
      return Not_locked;

    return Switch_lock::test();
  }

  void NO_INSTRUMENT clear()
  {
    if (EXPECT_FALSE( ! threading_system_active) ) // still initializing?
      return;

    Switch_lock::clear();
  }

  void set(Status s)
  {
    if (!s)
      clear();
  }

  Context* NO_INSTRUMENT lock_owner() const
  {
    if (EXPECT_FALSE( ! threading_system_active) ) // still initializing?
      return current();

    return Switch_lock::lock_owner();
  }
};

#undef NO_INSTRUMENT
#define NO_INSTRUMENT

