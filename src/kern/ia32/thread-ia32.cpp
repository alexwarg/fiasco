/*
 * Fiasco Thread Code
 */
INTERFACE [ia32,amd64]:

#include "trap_state.h"

class Idt_entry;

EXTENSION class Thread
{
private:
  /**
   * Return code segment used for exception reflection to user mode
   */
  static Mword exception_cs();
};

//----------------------------------------------------------------------------
IMPLEMENTATION [ia32,amd64]:

#include "config.h"
#include "cpu.h"
#include "cpu_lock.h"
#include "gdt.h"
#include "idt.h"
#include "ipi.h"
#include "mem_layout.h"
#include "logdefs.h"
#include "paging.h"
#include "processor.h"		// for cli/sti
#include "regdefs.h"
#include "std_macros.h"
#include "thread.h"
#include "timer.h"
#include "trap_state.h"
#include "exc_table.h"

Trap_state::Handler Thread::nested_trap_handler FIASCO_FASTCALL;

IMPLEMENT
Thread::Thread(Ram_quota *q)
: _quota(q),
  _del_observer(0)
{
  assert (state() == 0);

  inc_ref();
  _cpu_state.space.space(Kernel_task::kernel_task());

  if (Config::Stack_depth)
    std::memset((char*)this + sizeof(Thread), '5',
		Thread::Size-sizeof(Thread)-64);

  _magic          = magic;
  _recover_jmpbuf = 0;

  prepare_switch_to(&user_invoke);

  arch_init();

  alloc_eager_fpu_state();

  state.add_dirty(Thread_dead);

  // ok, we're ready to go!
}

PRIVATE static inline
Mword
Thread::sanitize_user_flags(Mword flags)
{ return (flags & ~(EFLAGS_IOPL | EFLAGS_NT)) | EFLAGS_IF; }


extern "C" FIASCO_FASTCALL
void
thread_restore_exc_state()
{
  current_thread()->restore_exc_state();
}

PRIVATE static
void
Thread::print_page_fault_error(Mword e)
{
  printf("%lx", e);
}

//----------------------------------------------------------------------------
IMPLEMENTATION [(ia32 || amd64) && cpu_local_map]:

PUBLIC inline
bool
Thread::update_local_map(Address pfa, Mword /*error_code*/)
{
  // This function assumes 4-level paging on AMD64. The page map level 4 table
  // is indexed by bits 47..39 of a linear address. Thus each entry covers 512G.
  static_assert(255 == (Mem_layout::User_max >> 39),
                "Mem_layout::User_max must lie in 512G slot 255.");
  // 512G slot 259 is used for context-specific kernel data.
  static_assert(259 == ((Mem_layout::Io_bitmap >> 39) & 0x1ff),
                "Mem_layout::Io_bitmap must lie in 512G slot 259.");
  static_assert(259 == ((Mem_layout::Caps_start >> 39) & 0x1ff),
                "Mem_layout::Caps_start must lie in 512G slot 259.");
  static_assert(259 == (((Mem_layout::Caps_end - 1) >> 39) & 0x1ff),
                "Mem_layout::Caps_end - 1 must lie in 512G slot 259.");

  unsigned idx = (pfa >> 39) & 0x1ff;
  if (EXPECT_FALSE((idx > 255) && idx != 259))
    return false;

  auto *m = Kmem::pte_map();
  if (EXPECT_FALSE(m->operator [](idx)))
    return false;

  auto s = Kmem::current_cpu_udir()->walk(Virt_addr(pfa), 0);
  assert (!s.is_valid());
  auto r = vcpu_aware_space()->dir()->walk(Virt_addr(pfa), 0);
  if (EXPECT_FALSE(!r.is_valid()))
    return false;

  m->set_bit(idx);
  *s.pte = *r.pte;
  return true;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [(ia32 || amd64) && !cpu_local_map]:

PUBLIC inline
bool
Thread::update_local_map(Address, Mword)
{ return false; }

//----------------------------------------------------------------------------
IMPLEMENTATION [ia32 || amd64]:


PROTECTED static inline
void
Thread::save_fpu_state_to_utcb(Trap_state *, Utcb *)
{}

//----------------------------------------------------------------------------
IMPLEMENTATION [ia32 || amd64]:

#include <feature.h>
KIP_KERNEL_FEATURE("segments");

PROTECTED inline
void
Thread::vcpu_resume_user_arch()
{}

PRIVATE inline
L4_msg_tag
Thread::sys_gdt_x86(L4_msg_tag tag, Utcb const *utcb, Utcb *out)
{
  // if no words given then return the first gdt entry
  if (EXPECT_FALSE(tag.words() == 1))
    {
      out->values[0] = Gdt::gdt_user_entry1 >> 3;
      return Kobject_iface::commit_result(0, 1);
    }

  unsigned idx = 0;

  for (unsigned entry_number = utcb->values[1];
      entry_number < _cpu_state.gdt_user_entries.Num
      && Utcb::val_idx(Utcb::val64_idx(2) + idx) < tag.words();
      ++idx, ++entry_number)
    {
      Gdt_entry d = access_once((Gdt_entry *)&utcb->val64[Utcb::val64_idx(2) + idx]);
      if (d.unsafe())
        return Kobject_iface::commit_result(-L4_err::EInval);

      _cpu_state.gdt_user_entries[entry_number] = d;
    }

  if (this == current_thread())
    _cpu_state.gdt_user_entries.load();

  return Kobject_iface::commit_result((utcb->values[1] << 3) + Gdt::gdt_user_entry1 + 3);
}

//----------------------------------------------------------------------------
IMPLEMENTATION [(ia32,amd64) && !io]:

PRIVATE inline
int
Thread::handle_io_page_fault(Trap_state *)
{ return 0; }

PRIVATE inline
bool
Thread::get_ioport(Address /*eip*/, Trap_state * /*ts*/,
                   unsigned * /*port*/, unsigned * /*size*/)
{ return false; }


//---------------------------------------------------------------------------
IMPLEMENTATION[ia32 || amd64]:

#include "fpu.h"
#include "fpu_alloc.h"
#include "fpu_state.h"
#include "gdt.h"
#include "globalconfig.h"
#include "idt.h"
#include "keycodes.h"
#include "simpleio.h"
#include "static_init.h"
#include "terminate.h"

IMPLEMENT static inline NEEDS ["gdt.h"]
Mword
Thread::exception_cs()
{
  return Gdt::gdt_code_user | Gdt::Selector_user;
}

/**
 * The ia32 specific part of the thread constructor.
 */
PRIVATE inline NEEDS ["gdt.h"]
void
Thread::arch_init()
{
  // clear out user regs that can be returned from the thread_ex_regs
  // system call to prevent covert channel
  Entry_frame *r = regs();
  r->flags(EFLAGS_IOPL_K | EFLAGS_IF | 2);	// ei
  r->cs(Gdt::gdt_code_user | Gdt::Selector_user);
  r->ss(Gdt::gdt_data_user | Gdt::Selector_user);

  r->sp(0);
  // after cs initialisation as ip() requires proper cs
  r->ip(0);
}


/** A C interface for Context::handle_fpu_trap, callable from assembly code.
    @relates Context
 */
// The "FPU not available" trap entry point
extern "C"
int
thread_handle_fputrap()
{
  LOG_TRAP_N(7);

  return current_thread()->switchin_fpu();
}
PROTECTED inline
int
Thread::sys_control_arch(Utcb const *, Utcb *)
{
  return 0;
}

