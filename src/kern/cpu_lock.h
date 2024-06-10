#pragma once

#include "types.h"
#include "processor.h"

/**
 * Global CPU lock. When held, IRQs are disabled on the current CPU
 * (preventing nested IRQ handling, and preventing the current
 * thread from being preempted).  It must only be held for very short
 * amounts of time.
 *
 * A generic (cli, sti) implementation of the lock can be found in
 * cpu_lock-generic.cpp.
 */
class Cpu_lock
{
public:
  /// The return type of test methods
  typedef Mword Status;

  enum : Status { Not_locked = 0 };

  Cpu_lock(const Cpu_lock&) = delete;

  /// ctor.
  Cpu_lock() = default;

  /**
   * Test if the lock is already held.
   * @return 0 if the lock is not held, not 0 if it already is held.
   */
  Status test() const
  {
    return !Proc::interrupts();
  }

  /**
   * Acquire the CPU lock.
   * The CPU lock disables IRQs. It should be held only for a very
   * short amount of time.
   */
  void lock()
  {
    Proc::cli();
  }

  /**
   * Release the CPU lock.
   */
  void clear()
  {
    Proc::sti();
  }

  /**
   * Acquire the CPU lock and return the old status.
   * @return something else than 0 if the lock was already held and
   *   0 if it was not held.
   */
  Status test_and_set()
  {
    Status ret = test();
    lock();
    return ret;
  }

  /**
   * Clear the CPU lock and return the old status.
   * @return something else than 0 if the lock was held before and
   *   0 if it was not held.
   */
  Status test_and_clear()
  {
    Status ret = test();
    clear();
    return ret;
  }


  /**
   * Set the CPU lock according to the given status.
   * @param state the state to set (0 clear, else lock).
   */
  void set(Status state)
  {
    if (state)
      lock();
    else
      clear();
  }
};

/**
 * The global CPU lock, contains the locking data necessary for some
 * special implementations.
 */
extern Cpu_lock cpu_lock;

