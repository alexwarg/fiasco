
#include <handle_pagefault.h>
#include <traps_bits.h>
#include <thread.h>
#include <kmem.h>
#include <exc_table.h>
#include <idt.h>
#include <kernel_task.h>
#include <dbg_stack.h>
#include <kernel_console.h>
#include <thread_vcpu.h>
#include <terminate.h>
#include <keycodes.h>
#include <kdb_ke.h>
#include <warn.h>
#include <globalconfig.h>
#include <std_macros.h>
#include <log_pagefault.h>
#include <traps_local_map.h>

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

static unsigned
check_io_bitmap_delimiter_fault(Thread *ct, Trap_state *ts)
{
  // check for page fault at the byte following the IO bitmap
  if (ts->_trapno == 14           // page fault?
      && (ts->_err & 4) == 0         // in supervisor mode?
      && ts->ip() <= Mem_layout::User_max   // delimiter byte accessed?
      && (ts->_cr2 == Mem_layout::Io_bitmap + Mem_layout::Io_port_max / 8))
    {
      Mem_space *m = ct->mem_space();
      // page fault in the first byte following the IO bitmap
      // map in the cpu_page read_only at the place
      Mem_space::Status result =
	m->v_insert(
	    Mem_space::Phys_addr(m->virt_to_phys_s0((void*)Kmem::io_bitmap_delimiter_page())),
	    Virt_addr(Mem_layout::Io_bitmap + Mem_layout::Io_port_max / 8),
	    Mem_space::Page_order(Config::PAGE_SHIFT),
	    Page::Attr::kern_global(Page::Rights::R()));

      switch (result)
	{
	case Mem_space::Insert_ok:
	  return 1; // success
	case Mem_space::Insert_err_nomem:
	  // kernel failure, translate this into a general protection
	  // violation and hope that somebody handles it
	  ts->_trapno = 13;
	  ts->_err    =  0;
	  return 0; // fail
	default:
	  // no other error code possible
	  assert (false);
	}
    }

  return 1;
}


/**
 * Chech whether the current trap is #GP caused by an unknown IRQ.
 *
 * If the current trap is a #GP caused by a non-present entry in the IDT, we
 * consider it an unknown interrupt which we acknowledge and ignore.
 *
 * Such unknown interrupts are usually caused by the firmware or the boot
 * loader configuring an interrupt source (e.g. the local APIC timer) and
 * failing to deactivate the source. The interrupt is then asserted while
 * interrupts are masked and there is no fail-safe way to deassert it.
 *
 * \param ts  Trap state.
 *
 * \retval 0  Unknown interrupt detected, the trap should be ignored.
 * \retval 1  The trap should be processed.
 */
inline int check_unknown_irq(Trap_state *ts)
{
  // Check for #GF and trap caused by invalid IDT entry (bit 1).
  if (ts->_trapno == 13 && (ts->_err & 2) == 2)
    {
      // Interrupt vector derived from the IDT selector index.
      Mword vector = (ts->_err & 0xffff) >> 3;

      // Ignore non-IRQ interrupt vectors.
      if (vector < 32 || vector >= Idt::_idt_max)
        return 1;

      if (!Idt::get(vector).present())
        {
          WARN("Unknown interrupt vector %lu, ignoring\n", vector);
          Apic::irq_ack();
          return 0;
        }
    }

  return 1;
}

inline int
handle_io_page_fault(Thread *ct, Trap_state *ts)
{
  Address eip = ts->ip();
  if (!check_io_bitmap_delimiter_fault(ct, ts))
    return 0;

  // Check for IO page faults. If we got exception #14, the IO bitmap page is
  // not available. If we got exception #13, the IO bitmap is available but
  // the according bit is set. In both cases we have to dispatch the code at
  // the faulting eip to determine the IO port and send an IO flexpage to our
  // pager. If it was a page fault, check the faulting address to prevent
  // touching userland.
  if (eip <= Mem_layout::User_max &&
      ((ts->_trapno == 13 && (ts->_err & 7) == 0) ||
       (ts->_trapno == 14 && Kmem::is_io_bitmap_page_fault(ts->_cr2))))
    {
      ts->_cr2 = 0;
      ts->_trapno = 13;
      ts->_err = 0;
      ct->recover_jmp_buf(nullptr);
      if (ct->send_exception(ts))
        return 1;
      else
        return 2; // fail, don't send exception again
    }
  return 0; // fail
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
      if (!check_unknown_irq(ts))
        return 0;

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

  switch (handle_io_page_fault(c, ts))
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


/**
 * Check if the pagefault occurred at a special place: At certain places in the
 * kernel (Mem_layout::read_special_safe()), we want to ensure that a specific
 * address is mapped.
 * The regular case is "mapped", the exception or slow case is "not mapped".
 * The fastest way to check this is to touch into the memory. If there is no
 * mapping for the address we get a pagefault. The pagefault handler sets the
 * carry and/or the zero flag which can be detected by the faulting code.
 *
 * @param regs  Pagefault return frame.
 * @returns False or true whether this was a pagefault at a special region or
 *          not. On true, the return frame got the carry flag and/or the zero
 *          flag set (depending on the architecture).
 */
inline bool
pagein_tcb_request(Return_frame *regs)
{
  if (!IS_ENABLED(CONFIG_VIRT_OBJ_SPACE))
    return false;

  // Counterpart: Mem_layout::read_special_safe()
  unsigned long new_ip = regs->ip();
  if (*(Unsigned8*)new_ip == 0x48) // REX.W
    new_ip += 1;

  Unsigned16 op = *(Unsigned16*)new_ip;
  if ((op & 0xc0ff) == 0x8b)
    {
      regs->ip(new_ip + 2);
      // stack layout:
      //         user eip
      //         PF error code
      // reg =>  eax/rax
      //         ecx/rcx
      //         edx/rdx
      //         ...
      Mword *reg = reinterpret_cast<Mword*>(regs) - 2 - Return_frame::Pf_ax_offset;
      assert((op >> 11) <= 2);
      reg[-(op>>11)] = 0; // op==0 => eax, op==1 => ecx, op==2 => edx

      // tell program that a pagefault occurred we cannot handle
      regs->flags(regs->flags() | 0x41); // set carry and zero flag in EFLAGS
      return true;
    }

  return false;
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

extern "C" FIASCO_FASTCALL
int
thread_page_fault(Address pfa, Mword error_code, Address ip, Mword flags,
		  Return_frame *regs);

/**
 * The low-level page fault handler called from entry.S.  We're invoked with
 * interrupts turned off.  Apart from turning on interrupts in almost
 * all cases (except for kernel page faults in TCB area), just forwards
 * the call to Thread::handle_page_fault().
 * @param pfa page-fault virtual address
 * @param error_code CPU error code
 * @return true if page fault could be resolved, false otherwise
 */
int
thread_page_fault(Address pfa, Mword error_code, Address ip, Mword /*flags*/,
		  Return_frame *regs)
{

  // XXX: need to do in a different way, if on debug stack e.g.
#if 0
  // If we're in the GDB stub -- let generic handler handle it
  if (EXPECT_FALSE (!in_context_area((void*)Proc::stack_pointer())))
    return false;
#endif

  Thread *t = current_thread();

  if (update_local_map(t, pfa, error_code))
    return 1;

  Log::page_fault(pfa, error_code, ip);

  // Check for page fault in IO bit map or in delimiter byte behind IO bitmap
  // assume it is caused by an input/output instruction and fall through to
  // handle_slow_trap
  if (EXPECT_FALSE(Kmem::is_io_bitmap_page_fault(pfa)))
    return 0;

  // Pagefault in user mode or interrupts were enabled
  if (EXPECT_TRUE(PF::is_usermode_error(error_code)))
    {
      if (Thread_vcpu::vcpu_pagefault(t, pfa, error_code, ip))
        return 1;

      if (Mem_layout::in_kernel(pfa))
        return 0;

      Proc::sti();
      return handle_user_space_page_fault(t, pfa, error_code);
    }

  // Check for page fault in user memory area
  if (!Mem_layout::in_kernel(pfa))
    {
      Proc::sti();
      return handle_user_space_page_fault(t, pfa, error_code);
    }

  if (Mem_layout::is_caps_area(pfa))
    {
      // Test for special case -- see function documentation
      if (pagein_tcb_request(regs))
        return 2;

      printf("Fiasco BUG: Invalid CAP access (ip=%lx, pfa=%lx)\n", ip, pfa);
      kdb_ke("Fiasco BUG: Invalid access to Caps area");
      return 0;
    }

  WARN("No page-fault handler for 0x%lx, error 0x%lx, ip " L4_PTR_FMT "\n",
        pfa, error_code, ip);

  t->do_recover_jmp_buf();
  return 0;
}

extern "C" int thread_handle_fputrap();

int
thread_handle_fputrap()
{
  LOG_TRAP_N(7);

  return current_thread()->switchin_fpu();
}

