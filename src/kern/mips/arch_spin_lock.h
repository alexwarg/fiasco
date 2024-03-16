#pragma once

template<typename SL>
class Arch_spin_lock
{
public:
  static_assert(sizeof(typename SL::Lock_type) >= sizeof(Unsigned32),
                "unsupported spin-lock type for MIPS");

  enum { Arch_lock = 2 };

  void lock_arch() noexcept
  {
    using Lock_t = typename SL::Lock_type;
    Lock_t dummy, tmp;

    __asm__ __volatile__(
        "   .set    push            \n"
        "   .set    noreorder       \n"
        "1: lw      %[d], %[lock]   \n"
        "   andi    %[d], %[d], 2   \n"
        "   bnez    %[d], 1b        \n"
        "     nop                   \n"
        "   ll      %[d], %[lock]   \n"
        "   andi    %[tmp], %[d], 2 \n" /* Arch_lock == #2 */
        "   bnez    %[tmp], 1b      \n" /* branch if lock already taken */
        "     ori   %[d], %[d], 2   \n" /* Arch_lock == #2 */
        "   sc      %[d], %[lock]   \n" /* acquire lock atomically */
        "   beqz    %[d], 1b        \n" /* branch if failed */
        "     nop                  \n"
        "   .set    pop             \n"
        : [lock] "+m" (static_cast<SL *>(this)->_lock), [tmp] "=&r" (tmp), [d] "=&r" (dummy)
        : : "memory");
  }

  void unlock_arch() noexcept
  {
    Mem::mp_mb();
    static_cast<SL *>(this)->_lock &= ~Arch_lock;
  }

};

