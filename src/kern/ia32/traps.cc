
#include <traps_bits.h>
#include <thread.h>
#include <kmem.h>
#include <exc_table.h>
#include <idt.h>
#include <kernel_task.h>
#include <dbg_stack.h>
#include <kernel_console.h>
#include <terminate.h>
#include <keycodes.h>
#include <globalconfig.h>

#ifndef CONFIG_JDB
/** There is no nested trap handler if both jdb and kdb are disabled.
 * Important: We don't need the nested_handler_stack here.
 */
inline int
call_nested_trap_handler(Trap_state *)
{ return -1; }
#endif

static bool
handle_kernel_exc(Trap_state *ts)
{
  for (auto const &e: Exc_entry::table())
    {
      if (e.ip != ts->ip())
        continue;

      if (e.handler)
        {
          if (0)
            printf("fixup exception(h): %ld: ip=%lx -> handler %p fixup %lx ts @ %p -> %lx\n",
                   ts->trapno(), ts->ip(), e.handler, e.fixup, ts, reinterpret_cast<Mword const *>(ts)[-1]);
          if (0)
            ts->dump();

          return e.handler(&e, ts);
        }
      else if (e.fixup)
        {
          if (0)
            printf("fixup exception: %ld: ip=%lx -> fixup %lx\n",
                   ts->trapno(), ts->ip(), e.fixup);
          ts->ip(e.fixup);
          return true;
        }
      else
        return false;
    }
  return false;
}

inline void
check_f00f_bug(Trap_state *ts)
{
  // If we page fault on the IDT, it must be because of the F00F bug.
  // Figure out exception slot and raise the corresponding exception.
  // XXX: Should we also modify the error code?
  if (ts->_trapno == 14		// page fault?
      && ts->_cr2 >= Idt::idt()
      && ts->_cr2 <  Idt::idt() + Idt::_idt_max * 8)
    ts->_trapno = (ts->_cr2 - Idt::idt()) / 8;
}

inline int
handle_not_nested_trap(Trap_state *ts)
{
  // no kernel debugger present
  printf(" %p IP=" L4_PTR_FMT " Trap=%02lx [Ret/Esc]\n",
	 current(), ts->ip(), ts->_trapno);

  int r;
  // cannot use normal getchar because it may block with hlt and irq's
  // are off here
  while ((r = Kconsole::console()->getchar(false)) == -1)
    Proc::pause();

  if (r == KEY_ESC)
    terminate (1);

  return 0;
}

/**
 * The global trap handler switch.
 * This function handles CPU-exception reflection, int3 debug messages,
 * kernel-debugger invocation, and thread crashes (if a trap cannot be
 * handled).
 * @param state trap state
 * @return 0 if trap has been consumed by handler;
 *          -1 if trap could not be handled.
 */
inline int
handle_slow_trap(Thread *c, Trap_state *ts)
{
  int from_user = ts->cs() & 3;

  if (EXPECT_FALSE(ts->_trapno == 0xee)) //debug IPI
    {
      Ipi::eoi(Ipi::Debug, current_cpu());
      goto generic_debug;
    }

  if (EXPECT_FALSE(ts->_trapno == 2))
    goto generic_debug;        // NMI always enters kernel debugger

  if (from_user && c->space_ref()->user_mode())
    {
      if (ts->_trapno == 14 && Kmem::is_io_bitmap_page_fault(ts->_cr2))
        {
	  ts->_trapno = 13;
	  ts->_err    = 0;
        }

      if (c->send_exception(ts))
	goto success;
    }

  // XXX We might be forced to raise an exception. In this case, our return
  // CS:IP points to leave_by_trigger_exception() which will trigger the
  // exception just before returning to userland. But if we were inside an
  // IPC while we was ex-regs'd, we will generate the 'exception after the
  // syscall' _before_ we leave the kernel.
  if (ts->_trapno == 13 && (ts->_err & 6) == 6)
    goto check_exception;

  LOG_TRAP;

  if (!check_trap13_kernel(ts))
    return 0;

  if (EXPECT_FALSE(!from_user))
    {
      if (handle_kernel_exc(ts))
        goto success;

      // get also here if a pagefault was not handled by the user level pager
      if (ts->_trapno == 14)
        goto check_exception;

      goto generic_debug;      // we were in kernel mode -- nothing to emulate
    }

  if (EXPECT_FALSE(ts->_trapno == 0xffffffff))
    goto generic_debug;        // debugger interrupt

  check_f00f_bug(ts);

  // so we were in user mode -- look for something to emulate

  // We continue running with interrupts off -- no sti() here. But
  // interrupts may be enabled by the pagefault handler if we get a
  // pagefault in peek_user().

  // Set up exception handling.  If we suffer an un-handled user-space
  // page fault, kill the thread.
  jmp_buf pf_recovery;
  unsigned error;
  if (EXPECT_FALSE ((error = setjmp(pf_recovery)) != 0) )
    {
      WARN("%p killed:\n"
           "\033[1mUnhandled page fault, code=%08x\033[m\n", c, error);
      goto fail_nomsg;
    }

  c->recover_jmp_buf(&pf_recovery);

  switch (c->handle_io_page_fault(ts))
    {
    case 1:
      c->recover_jmp_buf(nullptr);
      goto success;
    case 2:
      c->recover_jmp_buf(nullptr);
      goto fail;
    default:
      break;
    }

  c->recover_jmp_buf(nullptr);

check_exception:
  // see kdb_ke(), kdb_ke_nstr(), kdb_ke_nsequence()
  if (!from_user && (ts->_trapno == 3))
    goto generic_debug;

  // send exception IPC if requested
  if (c->send_exception(ts))
    goto success;

  // privileged tasks also may invoke the kernel debugger with a debug
  // exception
  if (ts->_trapno == 1)
    goto generic_debug;


fail:
  // can't handle trap -- kill the thread
  WARN("%p killed:\n"
       "\033[1mUnhandled trap \033[m\n", c);

fail_nomsg:
  if (Warn::is_enabled(Warning))
    ts->dump();

  c->kill();
  return 0;

success:
  c->recover_jmp_buf(nullptr);
  return 0;

generic_debug:
  c->recover_jmp_buf(nullptr);

  if (!Thread::nested_trap_handler)
    return handle_not_nested_trap(ts);

  return call_nested_trap_handler(ts);
}




/** The catch-all trap entry point.  Called by assembly code when a 
    CPU trap (that's not specially handled, such as system calls) occurs.
    Just forwards the call to Thread::handle_slow_trap().
    @param state trap state
    @return 0 if trap has been consumed by handler;
           -1 if trap could not be handled.
 */
extern "C" FIASCO_FASTCALL
int thread_handle_trap(Trap_state *ts, Cpu_number);

[[gnu::flatten]]
int
thread_handle_trap(Trap_state *ts, Cpu_number)
{
  return handle_slow_trap(current_thread(), ts);
}


