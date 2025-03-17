
#include <thread.h>
#include <cstdio>

[[noreturn]] void
Thread::user_invoke()
{
  user_invoke_generic();
  assert (current()->state() & Thread_ready);

  auto *ct = current_thread();
  auto *regs = ct->regs();
  Trap_state *ts = nonull_static_cast<Trap_state*>
    (nonull_static_cast<Return_frame*>(regs));

  static_assert(sizeof(ts->r[0]) == sizeof(Mword), "Size mismatch");
  Mem::memset_mwords(&ts->r[0], 0, cxx::size(ts->r));

  if (ct->space()->is_sigma0())
    ts->r[0] = Kmem::kdir->virt_to_phys((Address)Kip::k());

  // load KIP syscall code into r1/x1 to allow user processes to
  // do syscalls even without access to the KIP.
  ts->r[1] = reinterpret_cast<Mword *>(Kip::k())[0x100];

  if (ct->exception_triggered())
    ct->_exc_cont.flags(regs, ct->_exc_cont.flags(regs)
                              | Proc::Status_always_mask);
  else
    regs->psr |= Proc::Status_always_mask;
  Proc::cli();
  extern char __return_from_user_invoke[];
  Entry::arm_fast_exit(ts, __return_from_user_invoke, ts);

  // never returns here
}

