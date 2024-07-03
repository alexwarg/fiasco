#pragma once

template<typename SL>
class Arch_spin_lock
{
public:
  enum { Arch_lock = 2 };

  void lock_arch() noexcept
  {
    typename SL::Lock_type dummy, tmp;
#define L(z)                       \
    __asm__ __volatile__ (           \
        "1: mov %[lock], %[tmp]  \n" \
        "   test $2, %[tmp]      \n" /* Arch_lock == #2 */ \
        "   jz 2f                \n" \
        "   pause                \n" \
        "   jmp 1b               \n" \
        "2: mov %[tmp], %[d]     \n" \
        "   or $2, %[d]          \n" \
        "   lock; cmpxchg %[d], %[lock]  \n" \
        "   jnz 1b                \n" \
        : [d] "=&"#z (dummy), [tmp] "=&a" (tmp), [lock] "+m" (static_cast<SL *>(this)->_lock))

    if (sizeof(typename SL::Lock_type) > sizeof(char))
      L(r);
    else
      L(q);
#undef L

    Mem::mp_acquire();
  }

  void unlock_arch() noexcept
  {
    Mem::mp_release();

    static_cast<SL *>(this)->_lock &= ~Arch_lock;
  }
};
