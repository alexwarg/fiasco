#include <thread.h>

[[noreturn]] void
Thread::user_invoke()
{
  user_invoke_generic();
  Mword cx = 0;

  if (current()->space()->is_sigma0())
    cx = Kmem::virt_to_phys(Kip::k());

  asm volatile
    ("  movl %%eax,%%esp \n"    // set stack pointer to regs structure
     "  movl %%edx,%%es  \n"
     "  movl %%edx,%%ds  \n"
     "  xorl %%eax,%%eax \n"    // clean out user regs
     "  xorl %%edx,%%edx \n"
     "  xorl %%esi,%%esi \n"
     "  xorl %%edi,%%edi \n"
     "  xorl %%ebx,%%ebx \n"
     "  xorl %%ebp,%%ebp \n"
     "  iret             \n"
     :                          // no output
     : "a" (nonull_static_cast<Return_frame*>(current()->regs())),
       "d" (Gdt::gdt_data_user | Gdt::Selector_user),
       "c" (cx)
     : "memory"
     );

  __builtin_unreachable();
  // never returns here
}

