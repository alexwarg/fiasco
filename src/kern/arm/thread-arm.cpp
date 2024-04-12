INTERFACE [arm]:

class Trap_state;

EXTENSION class Thread
{
public:
  static void init_per_cpu(Cpu_number cpu, bool resume);
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include <cassert>
#include <cstdio>

#include "globals.h"
#include "kmem.h"
#include "mem_op.h"
#include "static_assert.h"
#include "thread_state.h"
#include "types.h"
#include "warn.h"

#include <thread_vcpu.h>

PRIVATE static
void
Thread::print_page_fault_error(Mword e)
{
  char const *const excpts[] =
    { "reset","undef. insn", "swi", "pref. abort", "data abort",
      "XXX", "XXX", "XXX" };

  unsigned ex = (e >> 20) & 0x07;

  printf("(%lx) %s, %s(%c)",e & 0xff, excpts[ex],
         (e & 0x00010000)?"user":"kernel",
         (e & 0x00020000)?'r':'w');
}

PUBLIC inline NEEDS[Thread::arm_fast_exit]
void FIASCO_NORETURN
Thread::vcpu_return_to_kernel(Mword ip, Mword sp, Vcpu_state *arg)
{
  extern char __iret[];
  Entry_frame *r = regs();

  r->ip(ip);
  r->sp(sp); // user-sp is in lazy user state and thus handled by
             // fill_user_state()
  fill_user_state();
  //load_tpidruro();

  // masking the illegal execution bit does not harm
  // on 32bit it is res/sbz
  r->psr &= ~(Proc::Status_thumb | (1UL << 20));

  // make sure the VMM executes in the correct mode
  if (Proc::Is_hyp)
    {
      r->psr_set_mode(Proc::Status_mode_user);
      r->psr |= 0x1c0; // mask PSTATE.{I,A,F}
    }

  assert(r->check_valid_user_psr());
  arm_fast_exit(nonull_static_cast<Return_frame*>(r), __iret, arg);

  // never returns here
}

IMPLEMENT_DEFAULT inline
void
Thread::init_per_cpu(Cpu_number, bool)
{}


//
// Public services
//

IMPLEMENT
void FIASCO_NORETURN
Thread::user_invoke()
{
  user_invoke_generic();
  assert (current()->state() & Thread_ready);

  auto *ct = current_thread();
  auto *regs = ct->regs();
  Trap_state *ts = nonull_static_cast<Trap_state*>
    (nonull_static_cast<Return_frame*>(regs));

  static_assert(sizeof(ts->r[0]) == sizeof(Mword), "Size mismatch");
  Mem::memset_mwords(&ts->r[0], 0, sizeof(ts->r) / sizeof(ts->r[0]));

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
  arm_fast_exit(ts, __return_from_user_invoke, ts);

  // never returns here
}

IMPLEMENT inline NEEDS["space.h", "types.h", "config.h"]
bool Thread::handle_sigma0_page_fault(Address pfa)
{
  Mem_space::Page_order size = mem_space()->sigma0_page_size();
  Virt_addr va = cxx::mask_lsb(Virt_addr(pfa), size);
  return mem_space()->v_insert(Mem_space::Phys_addr(va), va, size,
                               Mem_space::Attr(L4_fpage::Rights::URWX()))
    != Mem_space::Insert_err_nomem;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_lpae]:

PUBLIC static inline
Mword
Thread::is_debug_exception(Mword error_code, bool just_native_type = false)
{
  if (just_native_type)
    return (error_code & 0x4f) == 2;

  // LPAE type as already converted
  return (error_code & 0xc000003f) == 0x80000022;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae]:

PUBLIC static inline
Mword
Thread::is_debug_exception(Mword error_code, bool just_native_type = false)
{
  if (just_native_type)
    return ((error_code >> 26) & 0x3f) == 0x22;
  return (error_code & 0xc000003f) == 0x80000022;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include "trap_state.h"


/** Constructor.
    @post state() != 0
 */
IMPLEMENT
Thread::Thread(Ram_quota *q)
: _pager(Thread_ptr::Invalid),
  _exc_handler(Thread_ptr::Invalid),
  _quota(q),
  _del_observer(0)
{
  assert (state() == 0);

  inc_ref();
  _cpu_state.space.space(Kernel_task::kernel_task());

  if (Config::Stack_depth)
    std::memset((char *)this + sizeof(Thread), '5',
                Thread::Size - sizeof(Thread) - 64);

  // set a magic value -- we use it later to verify the stack hasn't
  // been overrun
  _magic = magic;
  _recover_jmpbuf = 0;

  prepare_switch_to(&user_invoke);

  // clear out user regs that can be returned from the thread_ex_regs
  // system call to prevent covert channel
  Entry_frame *r = regs();
  memset(r, 0, sizeof(*r));
  r->psr = Proc::Status_mode_user;

  alloc_eager_fpu_state();

  state.add_dirty(Thread_dead);

  // ok, we're ready to go!
}

IMPLEMENT inline
Mword
Thread::user_sp() const
{ return regs()->sp(); }

IMPLEMENT inline
void
Thread::user_sp(Mword sp)
{ return regs()->sp(sp); }

IMPLEMENT inline
Mword
Thread::user_flags() const
{ return 0; }

PRIVATE inline
void
Thread::save_fpu_state_to_utcb(Trap_state *ts, Utcb *u)
{
  (void)ts; (void)u;
#ifdef CONFIG_FPU
  char *esu = (char *)&u->values[21];
  Fpu::save_user_exception_state(state() & Thread_fpu_owner,  fpu_state(),
                                 ts, (Fpu::Exception_state_user *)esu);
#endif
}

PROTECTED inline
int
Thread::do_trigger_exception(Entry_frame *r, void *ret_handler)
{
  if (!_exc_cont.valid(r))
    {
      _exc_cont.activate(r, ret_handler);
      return 1;
    }
  return 0;
}

PROTECTED inline
int
Thread::sys_control_arch(Utcb const *, Utcb *)
{
  return 0;
}

PROTECTED inline NEEDS[Thread::set_tpidruro]
L4_msg_tag
Thread::invoke_arch(L4_msg_tag tag, Utcb const *utcb, Utcb *)
{
  switch (utcb->values[0] & Opcode_mask)
    {
    case Op_set_tpidruro_arm:
      return set_tpidruro(tag, utcb);
    default:
      return commit_result(-L4_err::ENosys);
    }
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && arm_v6plus]:

PROTECTED inline
void
Thread::vcpu_resume_user_arch()
{
  // just an experiment for now, we cannot really take the
  // user-writable register because user-land might already use it
  asm volatile("mcr p15, 0, %0, c13, c0, 2"
               : : "r" (utcb().access(true)->values[25]) : "memory");
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && (64bit || !arm_v6plus)]:

PROTECTED inline
void
Thread::vcpu_resume_user_arch()
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v6plus]:

PRIVATE inline
L4_msg_tag
Thread::set_tpidruro(L4_msg_tag tag, Utcb const *utcb)
{
  if (EXPECT_FALSE(tag.words() < 2))
    return commit_result(-L4_err::EInval);

  _cpu_state.tpidruro(utcb->values[1]);
  if (EXPECT_FALSE(state() & Thread_vcpu_enabled))
    arch_update_vcpu_state(vcpu_state().access());

  if (this == current_thread())
    _cpu_state.load_tpidruro();

  return commit_result(0);
}

PRIVATE inline
void
Thread::set_tpidruro(Trex const *t)
{
  _cpu_state.tpidruro(access_once(&t->tpidruro));
  if (this == current_thread())
    _cpu_state.load_tpidruro();
}

PRIVATE inline
void
Thread::store_tpidruro(Trex *t)
{
  t->tpidruro = _cpu_state.tpidruro();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_v6plus]:

PRIVATE inline
L4_msg_tag
Thread::set_tpidruro(L4_msg_tag, Utcb const *)
{
  return commit_result(-L4_err::EInval);
}

PRIVATE inline
void
Thread::set_tpidruro(Trex const *)
{}

PRIVATE inline
void
Thread::store_tpidruro(Trex *)
{}

//-----------------------------------------------------------------------------
IMPLEMENTATION [mp]:

#include "ipi.h"
#include "irq_mgr.h"
#include <sched.h>

EXTENSION class Thread
{
public:
  static void kern_kdebug_ipi_entry() asm("kern_kdebug_ipi_entry");
};

PUBLIC static inline NEEDS["ipi.h"]
void
Thread::handle_debug_remote_requests_irq()
{
  Ipi::eoi(Ipi::Debug, current_cpu());
  Thread::kern_kdebug_ipi_entry();
}

PUBLIC static inline
void
Thread::handle_timer_remote_requests_irq(Upstream_irq const *ui)
{
  Upstream_irq::ack(ui);
  current_thread()->handle_timer_interrupt();
}

class Thread_remote_rq_irq : public Irq_base
{
public:
  // we assume IPIs to be top level, no upstream IRQ chips
  void handle(Upstream_irq const *)
  { Sched<>::handle_remote_requests_irq(); }

  Thread_remote_rq_irq()
  {
    set_hit(&handler_wrapper<Thread_remote_rq_irq>);
    unmask();
  }

  void switch_mode(bool) override {}
};

class Thread_glbl_remote_rq_irq : public Irq_base
{
public:
  // we assume IPIs to be top level, no upstream IRQ chips
  void handle(Upstream_irq const *)
  { Thread::handle_global_remote_requests_irq(); }

  Thread_glbl_remote_rq_irq()
  {
    set_hit(&handler_wrapper<Thread_glbl_remote_rq_irq>);
    unmask();
  }

  void switch_mode(bool) override {}
};

class Thread_debug_ipi : public Irq_base
{
public:
  // we assume IPIs to be top level, no upstream IRQ chips
  void handle(Upstream_irq const *)
  { Thread::handle_debug_remote_requests_irq(); }

  Thread_debug_ipi()
  {
    set_hit(&handler_wrapper<Thread_debug_ipi>);
    unmask();
  }

  void switch_mode(bool) override {}
};

class Thread_timer_tick_ipi : public Irq_base
{
public:
  void handle(Upstream_irq const *ui)
  { Thread::handle_timer_remote_requests_irq(ui); }

  Thread_timer_tick_ipi()
  { set_hit(&handler_wrapper<Thread_timer_tick_ipi>); }

  void switch_mode(bool) override {}
};


//-----------------------------------------------------------------------------
IMPLEMENTATION [mp && !arm_single_ipi_irq && !irregular_gic]:

class Arm_ipis
{
public:
  Arm_ipis()
  {
    check(Irq_mgr::mgr->alloc(&remote_rq_ipi, Ipi::Request, false));
    check(Irq_mgr::mgr->alloc(&glbl_remote_rq_ipi, Ipi::Global_request, false));
    check(Irq_mgr::mgr->alloc(&debug_ipi, Ipi::Debug, false));
    check(Irq_mgr::mgr->alloc(&timer_ipi, Ipi::Timer, false));
  }

  Thread_remote_rq_irq remote_rq_ipi;
  Thread_glbl_remote_rq_irq glbl_remote_rq_ipi;
  Thread_debug_ipi debug_ipi;
  Thread_timer_tick_ipi timer_ipi;
};

static Arm_ipis _arm_ipis;

