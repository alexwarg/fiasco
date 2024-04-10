#pragma once

#include <types.h>
#include <config.h>
#include <config_tcbsize.h>
#include <processor.h>
#include <thread_state.h>

#include <cxx/atomic>
#include <cassert>

class Context;

class Context_base
{
public:
  enum
  {
    Size = THREAD_BLOCK_SIZE
  };

  // This virtual dtor enforces that Context / Thread / Context_base
  // all start at offset 0
  virtual ~Context_base() = 0;

  void set_current_cpu(Cpu_number cpu)
  {
    _cpu = cpu;
  }

  Cpu_number get_current_cpu() const
  {
    return _cpu;
  }

  struct State
  {
    cxx::atomic<Mword> s;

    Mword dirty() const noexcept
    { return s.load(cxx::memory_order_relaxed); }

    Mword operator () () const noexcept
    { return s.load(); }

    explicit operator Mword () const noexcept
    { return s.load(); }

    void change_dirty(Mword mask, Mword bits)
    {
      Mword old = s.load(cxx::memory_order_relaxed);
      while (!s.compare_exchange_weak(old, (old & mask) | bits,
              cxx::memory_order_relaxed))
        ;
    }

    /**
     * Atomically delete and add bits in state flags, provided the
     *        following rules apply (otherwise state is not changed at all):
     *        - Bits that are to be set must be clear in state or clear in mask
     *        - Bits that are to be cleared must be set in state
     * @param mask Bits not set in mask shall be deleted from state flags
     * @param bits Bits to be added to state flags
     * @return 1 if state was changed, 0 otherwise
     */
    Mword change_safely(Mword mask, Mword bits)
    {
      Mword old = s;
      do
        {
          if ((old & bits & mask) | (~old & ~mask))
            return 0;
        }
      while (!s.compare_exchange_weak(old, (old & mask) | bits));

      return 1;
    }

    /**
     * Atomically delete and add bits in state flags.
     * @param mask bits not set in mask shall be deleted from state flags
     * @param bits bits to be added to state flags
     */
    Mword change(Mword mask, Mword bits)
    {
      Mword old = s;
      while (!s.compare_exchange_weak(old, (old & mask) | bits))
        ;

      return old;
    }

    /**
     * Atomically add bits to state flags.
     * @param bits bits to be added to state flags
     * @return 1 if none of the bits that were added had been set before
     */
    void add(Mword bits)
    {
      s |= bits;
    }

    Mword operator |= (Mword bits)
    { return s |= bits; }

    void add_dirty(Mword bits)
    {
      s.fetch_or(bits, cxx::memory_order_relaxed);
    }

    /**
     * Atomically delete bits from state flags.
     * @param bits bits to be removed from state flags
     * @return 1 if all of the bits that were removed had previously been set
     */
    Mword del(Mword bits)
    {
      return s.fetch_and(~bits);
    }

    void del_dirty(Mword bits)
    {
      s.fetch_and(~bits, cxx::memory_order_relaxed);
    }

    /**
     * Check if the context is inactive, i.e. has not yet been started or was killed.
     * @return true if this context is inactive.
     */
    bool is_invalid() const
    {
      return s & Thread_dead;
    }
  };

  State state;

private:
  friend Cpu_number current_cpu();
  Cpu_number _cpu;
};

inline Context_base::~Context_base() {}

[[gnu::always_inline]] inline
Context *context_of(const void *ptr) noexcept
{
  return reinterpret_cast<Context *>
    (reinterpret_cast<unsigned long>(ptr) & ~(Context_base::Size - 1));
}

[[gnu::always_inline]] inline
Context *current() noexcept
{
  return context_of((void *)Proc::stack_pointer());
}

[[gnu::pure]] inline
Cpu_number current_cpu() noexcept
{
  return reinterpret_cast<Context_base *>(current())->_cpu;
}

/**
 * \brief Encapsulate an aggregate of Context.
 *
 * Allow to get a back reference to the aggregating Context object.
 */
class Context_member
{
public:
  Context_member() = default;
  Context_member(Context_member const &) = delete;
  Context_member(Context_member &&) = delete;
  Context_member &operator = (Context_member &&) = delete;
  ~Context_member() noexcept = default;

  /**
   * \brief Get the aggregating Context object.
   */
  Context *context() const noexcept
  { return context_of(this); }
};

