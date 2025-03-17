#include <thread.h>
#include <cstdio>
#include <cassert>

[[noreturn]] void
Thread::user_invoke()
{
  user_invoke_generic();
  assert(current()->state() & Thread_ready);
  auto ts = current()->regs();

  Proc::cli();

  ts->r[4] = 0;

  if (EXPECT_FALSE(current_thread()->mem_space()->is_sigma0()))
    ts->r[4] = Mem_layout::pmem_to_phys(Kip::k());

  // FIXME: do we really need this or should the user be
  // responsible for that
  //Mem_op::cache()->icache_invalidate_all();

  do
    {
      extern char ret_from_user_invoke[];
      register void *a0 __asm__("a0") = ts;
      register void const *ra __asm__("ra") = ret_from_user_invoke;
      __asm__ __volatile__ (
          ASM_ADDIU "  $sp, %[ts], -%[cfs]   \n"
          "jr          %[ra]                 \n"
          "nop                               \n"
          :
          : [ra] "r" (ra),
            [ts] "r" (a0),
            [cfs] "i" (ASM_WORD_BYTES * ASM_NARGSAVE));
    }
  while (0);

  __builtin_unreachable();
  // never returns
}


