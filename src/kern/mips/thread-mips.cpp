IMPLEMENTATION [mips]:

#include <cstdio>
#include <cassert>
#include "alternatives.h"
#include "asm_mips.h"
#include "cp0_status.h"
#include "kip.h"
#include "trap_state.h"
#include "processor.h"
#include "types.h"

#include <thread_vcpu.h>

PROTECTED inline
int
Thread::sys_control_arch(Utcb const *, Utcb *)
{
  return 0;
}

PRIVATE inline
int
Thread::cache_op(unsigned op, Address start, Address end)
{
  jmp_buf pf_recovery;
  if (setjmp(pf_recovery) != 0)
    {
      recover_jmp_buf(0);
      return -L4_err::EFault;
    }

  recover_jmp_buf(&pf_recovery);

  switch (op)
    {
    case 0x22:
      // this is invalidate only, however we do a flush
      // otherwise we would need to do a flush for incomplete
      // cachelines at the start end end of the range
    case 0x20:
      Mem_unit::dcache_flush(start, end);
      break;
    case 0x21:
      Mem_unit::dcache_clean(start, end);
      break;
    }

  recover_jmp_buf(0);
  return 0;
}


PROTECTED inline NEEDS["processor.h", Thread::sys_vz_save_state,
                       Thread::cache_op]
L4_msg_tag
Thread::invoke_arch(L4_msg_tag tag, Utcb const *utcb, Utcb *)
{
  switch (unsigned op = access_once(&utcb->values[0]) & Opcode_mask)
    {
    case 0x10: // set ULR op-code
      if (tag.words() < 2)
        return commit_result(-L4_err::EMsgtooshort);

      _cpu_state.ulr = access_once(&utcb->values[1]);
      if (current() == this)
        Proc::set_ulr(_cpu_state.ulr);
      return Kobject_iface::commit_result(0);

    case 0x14:
      return commit_result(sys_vz_save_state(tag, utcb));

    case 0x20:
    case 0x21:
    case 0x22:
      if (tag.words() < 3)
        return commit_result(-L4_err::EMsgtooshort);

      return commit_result(cache_op(op, access_once(&utcb->values[1]),
                                    access_once(&utcb->values[2])));

    default:
      return commit_result(-L4_err::ENosys);
    }
}

IMPLEMENT inline Mword Thread::user_sp() const    { return regs()->sp(); }
IMPLEMENT inline void  Thread::user_sp(Mword sp)  { regs()->sp(sp); }
IMPLEMENT inline Mword Thread::user_flags() const { return regs()->status; }
IMPLEMENT inline Mword Thread::user_ip() const    { return regs()->ip(); }
IMPLEMENT inline void  Thread::user_ip(Mword ip)  { regs()->ip(ip); }

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

  assert(state() == 0);

  inc_ref();
  _cpu_state.space.space(Kernel_task::kernel_task());

  if (Config::Stack_depth)
    std::memset((char*)this + sizeof(Thread), '5',
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
  r->status = Cp0_status::status_eret_to_user_ei(Cp0_status::read());

  alloc_eager_fpu_state();

  state.add_dirty(Thread_dead);
  // ok, we're ready to go!
}

IMPLEMENT inline
bool
Thread::pagein_tcb_request(Return_frame *)
{
  assert(false);
  return false;
}

// ERET to user mode
IMPLEMENT
void FIASCO_NORETURN
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
      Mword register a0 __asm__("a0") = (Mword)ts;
      Mword register ra __asm__("ra") = (Mword)ret_from_user_invoke;
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

IMPLEMENT inline NEEDS["space.h", "types.h", "config.h"]
bool Thread::handle_sigma0_page_fault(Address pfa)
{
  Mem_space::Page_order size = mem_space()->sigma0_page_size();
  Virt_addr va = cxx::mask_lsb(Virt_addr(pfa), size);
  return mem_space()->v_insert(Mem_space::Phys_addr(va), va, size,
                               Mem_space::Attr(L4_fpage::Rights::URWX()))
    != Mem_space::Insert_err_nomem;
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

PRIVATE static
void
Thread::print_page_fault_error(Mword e)
{
  printf("%s (%ld), %s(%c) (%lx)",
         Trap_state::exc_code_to_str(e), (e >> 2) & 0x1f,
         PF::is_usermode_error(e) ? "user" : "kernel",
         PF::is_read_error(e) ? 'r' : 'w', e);
}

//
// Public services
//
PUBLIC inline NEEDS[<cassert>, "cp0_status.h"]
void FIASCO_NORETURN
Thread::vcpu_return_to_kernel(Mword ip, Mword sp, void *arg)
{
  assert (cpu_lock.test());
  assert (current() == this);

  {
    Mword register a0 __asm__("a0") = (Mword)arg;
    Mword register t9 __asm__("t9") = (Mword)ip;
    asm volatile
      (".set push                     \n"
       ".set noat                     \n"
       "  mfc0  $1, $12               \n"
       "  ins   $1, %[status], 0, 8   \n"
       "  move  $29, %[sp]            \n"
       "  " ASM_MTC0 "  %[ip], $14    \n"
       "  mtc0  $1, $12               \n"
       "  ehb                         \n"
       "  eret                        \n"
       ".set pop                      \n"
       : : [status] "r" (Cp0_status::ST_USER_DEFAULT),
           [ip] "r" (t9), [sp] "r" (sp), [arg] "r" (a0)
      );
  }

  panic("__builtin_trap()");
}

extern "C" void leave_by_vcpu_upcall()
{
  Thread *c = current_thread();
  c->regs()->r[0] = 0; // reset continuation
  Vcpu_state *vcpu = c->vcpu_state().access();
  vcpu->_regs.s = *nonull_static_cast<Trap_state*>(c->regs());
  c->vcpu_return_to_kernel(vcpu->_entry_ip, vcpu->_entry_sp, c->vcpu_state().usr().get());
}

PRIVATE static inline
void
Thread::save_fpu_state_to_utcb(Trap_state *, Utcb *)
{}

PRIVATE static inline
bool FIASCO_WARN_RESULT
Thread::copy_utcb_to_ts(L4_msg_tag const &tag, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  // only a complete state will be used.
  if (EXPECT_FALSE(tag.words() < (sizeof(Trex) / sizeof(Mword))))
    return true;

  Trap_state *ts = (Trap_state*)rcv->_utcb_handler;
  Utcb *snd_utcb = snd->utcb().access();

  Trex const *r = reinterpret_cast<Trex const *>(snd_utcb->values);
  // this skips the eret/continuation work already
  ts->copy_and_sanitize(&r->s);
  rcv->_cpu_state.ulr = access_once(&r->ulr);
  if (rcv == current())
    Proc::set_ulr(rcv->_cpu_state.ulr);

  if (tag.transfer_fpu() && (rights & L4_fpage::Rights::CS()))
    snd->transfer_fpu(rcv);

  bool ret = transfer_msg_items(tag, snd, snd_utcb,
                                rcv, rcv->utcb().access(), rights);

  return ret;
}

PRIVATE static inline NEEDS["trap_state.h"]
bool FIASCO_WARN_RESULT
Thread::copy_ts_to_utcb(L4_msg_tag const &, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  Trap_state *ts = (Trap_state*)snd->_utcb_handler;

  {
    auto guard = lock_guard(cpu_lock);
    Utcb *rcv_utcb = rcv->utcb().access();
    Trex *r = reinterpret_cast<Trex *>(rcv_utcb->values);
    r->s = *ts;
    r->ulr = snd->_cpu_state.ulr;

    if (rcv_utcb->inherit_fpu() && (rights & L4_fpage::Rights::CS()))
      snd->transfer_fpu(rcv);

    __asm__ __volatile__ ("" : : "m"(*r));
  }
  return true;
}

//----------------------------------------------------------
IMPLEMENTATION [mips && !mips_vz]:

PRIVATE inline
int
Thread::sys_vz_save_state(L4_msg_tag, Utcb const *)
{ return -L4_err::ENosys; }

//----------------------------------------------------------
IMPLEMENTATION [mips && mips_vz]:

#include "cpu.h"
#include "vz.h"
PRIVATE inline
int
Thread::sys_vz_save_state(L4_msg_tag tag, Utcb const *utcb)
{
  if (tag.words() < 2)
    return -L4_err::EMsgtooshort;

  if (!(state() & Thread_ext_vcpu_enabled))
    return -L4_err::EInval;

  if (current() != this)
    return -L4_err::EInval;

  auto *v = vm_state(vcpu_state().kern());

  // must be saved already
  if (!(state() & Thread_vcpu_vz_owner))
    {
      // update the cause TI bit in this case
      if (utcb->values[1] & Vz::State::M_cause)
        v->update_cause_ti();
      return 0;
    }

  // always read the cause register when requested by the VMM
  if (utcb->values[1] & Vz::State::M_cause)
    v->current_cp0_map &= ~Vz::State::M_cause;

  // we have a bitmap in utcb->values[1] which state to save, however
  // we save the full state for now.
  v->save_full(Vz::owner.current().guest_id);
  return 0;
}


