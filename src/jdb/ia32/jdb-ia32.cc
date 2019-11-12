
#include <jdb_arch.h>
#include <jdb.h>

#include <globalconfig.h>

#include <cstring>
#include <csetjmp>
#include <cstdarg>
#include <climits>
#include <cstdlib>
#include <cstdio>
#include <simpleio.h>

#include <apic.h>
#include <boot_info.h>
#include <checksum.h>
#include <config.h>
#include <cpu.h>
#include <div32.h>
#include <initcalls.h>
#include <idt.h>
#include <io_apic.h>
#include <jdb_core.h>
#include <jdb_screen.h>
#include <kernel_console.h>
#include <keycodes.h>
#include <kernel_uart.h>
#include <kernel_task.h>
#include <kmem.h>
#include <koptions.h>
#include <logdefs.h>
#include <mem_layout.h>
#include <pc_i8259.h>
#include <push_console.h>
#include <processor.h>
#include <regdefs.h>
#include <static_init.h>
#include <terminate.h>
#include <thread.h>
#include <thread_state.h>
#include <timer.h>
#include <timer_tick.h>
#include <trap_state.h>
#include <watchdog.h>
#include <paging_bits.h>

#include <doublefault.h>
#include <dbg_stack.h>

char Jdb_ia32_base::_connected;			// Jdb::init() was done
// explicit single_step command
DEFINE_PER_CPU Per_cpu<char> Jdb_ia32_base::permanent_single_step;
volatile char Jdb_ia32_base::msr_test;		// = 1: trying to access an msr
volatile char Jdb_ia32_base::msr_fail;		// = 1: MSR access failed
DEFINE_PER_CPU Per_cpu<char> Jdb_ia32_base::code_ret; // current instruction is ret/iret
DEFINE_PER_CPU Per_cpu<char> Jdb_ia32_base::code_call;// current instruction is call
DEFINE_PER_CPU Per_cpu<char> Jdb_ia32_base::code_bra; // current instruction is jmp/jxx
DEFINE_PER_CPU Per_cpu<char> Jdb_ia32_base::code_int; // current instruction is int x

// special single step state
DEFINE_PER_CPU Per_cpu<Jdb_ia32_base::Step_state> Jdb_ia32_base::ss_state;
DEFINE_PER_CPU Per_cpu<int> Jdb_ia32_base::ss_level;  // current call level

const Unsigned8*Jdb_ia32_base::debug_ctrl_str;	// string+length for remote control of
int             Jdb_ia32_base::debug_ctrl_len;	// Jdb via enter_kdebugger("*#");

Unsigned16 Jdb_ia32_base::pic_status;
DEFINE_PER_CPU Per_cpu<unsigned> Jdb_ia32_base::apic_tpr;
DEFINE_PER_CPU Per_cpu<int> Jdb_ia32_base::jdb_irqs_disabled;

int  (*Jdb_ia32_base::bp_test_log_only)(Cpu_number cpu, Jdb_entry_frame *ef);
int  (*Jdb_ia32_base::bp_test_sstep)(Cpu_number cpu, Jdb_entry_frame *ef);
int  (*Jdb_ia32_base::bp_test_break)(Cpu_number cpu, Jdb_entry_frame *ef, String_buffer *buf);
int  (*Jdb_ia32_base::bp_test_other)(Cpu_number cpu, Jdb_entry_frame *ef, String_buffer *buf);


// available from the jdb_dump module
int jdb_dump_addr_task (Jdb_address addr, int level)
  __attribute__((weak));


STATIC_INITIALIZE_P(Jdb,JDB_INIT_PRIO);


FIASCO_INIT
void Jdb::init()
{
  if (Koptions::o()->opt(Koptions::F_nojdb))
    return;

  if (Koptions::o()->opt(Koptions::F_jdb_never_stop))
    never_break = 1;

  Trap_state::base_handler = &enter_jdb;

  // if esc_hack, serial_esc or watchdog enabled, set slow timer handler
  Idt::set_vectors_run();

  // disable lbr feature per default since it eats cycles on AMD Athlon boxes
  Cpu::boot_cpu()->lbr_enable(false);

  Kconsole::console()->register_console(push_cons());

  _connected = true;
  Dbf::may_enter_dbg = true;
}

DEFINE_PER_CPU static Per_cpu<Proc::Status> jdb_saved_flags;

// disable interrupts before entering the kernel debugger
void
Jdb::save_disable_irqs(Cpu_number cpu)
{
  if (!jdb_irqs_disabled.cpu(cpu)++)
    {
      // save interrupt flags
      jdb_saved_flags.cpu(cpu) = Proc::cli_save();

      if (cpu == Cpu_number::boot_cpu())
	{
	  Watchdog::disable();
	  pic_status = Pc_i8259().disable_all_save();
          if (Config::getchar_does_hlt_works_ok)
            Timer_tick::disable(Cpu_number::boot_cpu());
	}
      if (Io_apic::active() && Apic::is_present())
	{
	  apic_tpr.cpu(cpu) = Apic::tpr();
	  Apic::tpr(APIC_IRQ_BASE - 0x08);
	}

      if (cpu == Cpu_number::boot_cpu() && Config::getchar_does_hlt_works_ok)
	{
	  // set timer interrupt does nothing than wakeup from hlt
	  Timer_tick::set_vectors_stop();
	  Timer_tick::enable(Cpu_number::boot_cpu());
	}

    }

  if (cpu == Cpu_number::boot_cpu() && Config::getchar_does_hlt_works_ok)
    // explicit enable interrupts because the timer interrupt is
    // needed to wakeup from "hlt" state in getchar(). All other
    // interrupts are disabled at the pic.
    Proc::sti();
}

// restore interrupts after leaving the kernel debugger
void
Jdb::restore_irqs(Cpu_number cpu)
{
  if (!--jdb_irqs_disabled.cpu(cpu))
    {
      Proc::cli();

      if (Io_apic::active() && Apic::is_present())
	Apic::tpr(apic_tpr.cpu(cpu));

      if (cpu == Cpu_number::boot_cpu())
	{
	  Pc_i8259().restore_all(Jdb::pic_status);
	  Watchdog::enable();
	}

      // reset timer interrupt vector
      if (cpu == Cpu_number::boot_cpu() && Config::getchar_does_hlt_works_ok)
      	Idt::set_vectors_run();

      // reset interrupt flags
      Proc::sti_restore(jdb_saved_flags.cpu(cpu));
    }
}


static inline bool same_page(Address a1, Address a2)
{
  return Pg::trunc(a1) == Pg::trunc(a2);
}

static inline bool consecutive_pages(Address a1, Address a2)
{
  return Pg::trunc(a1) + Config::PAGE_SIZE == Pg::trunc(a2);
}

static inline bool same_or_consecutive_pages(Address a1, Address a2)
{
  return same_page(a1, a2) || consecutive_pages(a1, a2);
}

void
Jdb_ia32_base::peek_phys(Address phys, void *value, int width)
{
  assert(width > 0);
  assert(same_or_consecutive_pages(phys, phys + width - 1));

  Address virt = Kmem::map_phys_page_tmp(phys, 0);

  memcpy(value, (void*)virt, width);
}

void
Jdb_ia32_base::poke_phys(Address phys, void const *value, int width)
{
  assert(width > 0);
  assert(same_or_consecutive_pages(phys, phys + width - 1));

  Address virt = Kmem::map_phys_page_tmp(phys, 0);

  memcpy((void*)virt, value, width);
}

unsigned char *
Jdb::access_mem_task(Jdb_address addr, bool write)
{
  if (!Cpu::is_canonical_address(addr.addr()))
    return nullptr;

  bool is_sigma0 = false;

  Address phys;

  if (addr.is_kmem())
    {
      Address pdbr = Cpu::get_pdbr();
      Pdir *kdir = (Pdir *)Mem_layout::phys_to_pmem(pdbr);
      auto i = kdir->walk(Virt_addr(addr.addr()));
      if (!i.is_valid())
        return nullptr;

      if (!write || (i.attribs().rights & Page::Rights::W()))
        return (unsigned char *)addr.virt();

      phys = i.page_addr() | cxx::get_lsb(addr.addr(), i.page_order());
    }
  else if (addr.is_phys())
    phys = addr.phys();
  else
    {
      // user address (addr.have_space() == true), use temporary mapping
      is_sigma0 = addr.space()->is_sigma0();
      phys = Address(addr.space()->virt_to_phys(addr.addr()));

#ifndef CONFIG_CPU_LOCAL_MAP
      if (phys == ~0UL)
        phys = addr.space()->virt_to_phys_s0(addr.virt());
#endif
    }

  if (phys == ~0UL)
    return nullptr;

  Address virt = Kmem::map_phys_page_tmp(phys, is_sigma0);
  return reinterpret_cast<unsigned char *>(virt);
}

Address
Jdb_ia32_base::get_phys_address(Jdb_address addr)
{
  if (addr.is_null())
    return (Address)~0UL;

  if (addr.is_phys())
    return addr.phys();

  if (addr.is_kmem())
    return Kmem::virt_to_phys(addr.virt());

  return addr.space()->virt_to_phys_s0(addr.virt());
}

// The content of apdapter memory is not shown by default because reading
// memory-mapped I/O registers may confuse the hardware. We assume that all
// memory above the end of the RAM is adapter memory.
int
Jdb_ia32_base::is_adapter_memory(Jdb_address addr)
{
  if (addr.is_null())
    return false;

  Address phys = get_phys_address(addr);

  if (phys == ~0UL)
    return false;

  for (auto const &m: Kip::k()->mem_descs_a())
    if (m.type() == Mem_desc::Conventional && !m.is_virtual()
        && m.contains(phys))
      return false;

  return true;
}

#define WEAK __attribute__((weak))
extern "C" char in_slowtrap, in_page_fault, in_handle_fputrap;
extern "C" char in_interrupt, in_timer_interrupt, in_timer_interrupt_slow;
extern "C" char in_slow_ipc4 WEAK, in_slow_ipc5;
extern "C" char in_sc_ipc1 WEAK, in_sc_ipc2 WEAK;
#undef WEAK

// Try to guess the thread state of t by walking down the kernel stack and
// locking at the first return address we find.
Jdb_ia32_base::Guessed_thread_state
Jdb_ia32_base::guess_thread_state(Thread *t)
{
  Guessed_thread_state state = s_unknown;
  void **ktop = offset_cast<void**>(context_of(t->get_kernel_sp()), Context::Size);

  for (int i=-1; i>-26; i--)
    {
      if (ktop[i] != nullptr)
	{
	  if (ktop[i] == &in_page_fault)
	    state = s_pagefault;
	  if ((ktop[i] == &in_slow_ipc4) ||  // entry.S, int 0x30 log
	      (ktop[i] == &in_slow_ipc5) ||  // entry.S, sysenter log
#if defined (CONFIG_JDB_LOGGING)
	      (ktop[i] == &in_sc_ipc1)   ||  // entry.S, int 0x30
	      (ktop[i] == &in_sc_ipc2)   ||  // entry.S, sysenter
#endif
	     0)
	    state = s_ipc;
	  else if (ktop[i] == reinterpret_cast<void *>(Jdb::user_invoke_addr<Thread>()))
	    state = s_user_invoke;
	  else if (ktop[i] == &in_handle_fputrap)
	    state = s_fputrap;
	  else if (ktop[i] == &in_interrupt)
	    state = s_interrupt;
	  else if ((ktop[i] == &in_timer_interrupt) ||
		   (ktop[i] == &in_timer_interrupt_slow))
	    state = s_timer_interrupt;
	  else if (ktop[i] == &in_slowtrap)
	    state = s_slowtrap;
	  if (state != s_unknown)
	    break;
	}
    }

  if (state == s_unknown && (t->state() & Thread_ipc_mask))
    state = s_ipc;

  return state;
}

void
Jdb_ia32_base::set_single_step(Cpu_number cpu, int on)
{
  if (on)
    Jdb::entry_frame.cpu(cpu)->flags(Jdb::entry_frame.cpu(cpu)->flags() | EFLAGS_TF);
  else
    Jdb::entry_frame.cpu(cpu)->flags(Jdb::entry_frame.cpu(cpu)->flags() & ~EFLAGS_TF);

  permanent_single_step.cpu(cpu) = on;
}

#ifdef CONFIG_BIT32

// take a look at the code of the current thread eip
// set global indicators code_call, code_ret, code_bra, code_int
// This can fail if the current page is still not mapped
static void analyze_code(Cpu_number cpu)
{
  Jdb_entry_frame *entry_frame = Jdb::entry_frame.cpu(cpu);
  Space *task = Jdb::get_task(cpu);
  // do nothing if page not mapped into this address space
  if (entry_frame->ip()+1 > Kmem::user_max())
    return;

  Unsigned8 op1, op2;

  Jdb_addr<Unsigned8> insn_ptr(reinterpret_cast<Unsigned8*>(entry_frame->ip()), task);

  if (   !Jdb::peek(insn_ptr, op1)
      || !Jdb::peek(insn_ptr + 1, op2))
    return;

  if (op1 != 0x0f && op1 != 0xff)
    op2 = 0;

  Jdb_ia32_base::code_ret.cpu(cpu) =
	      (   ((op1 & 0xf6) == 0xc2)	// ret/lret /xxxx
	       || (op1 == 0xcf));		// iret

  Jdb_ia32_base::code_call.cpu(cpu) =
	      (   (op1 == 0xe8)			// call near
	       || ((op1 == 0xff)
	           && ((op2 & 0x30) == 0x10))	// call/lcall *(...)
	       || (op1 == 0x9a));		// lcall xxxx:xxxx

  Jdb_ia32_base::code_bra.cpu(cpu) =
	      (   ((op1 & 0xfc) == 0xe0)	// loop/jecxz
	       || ((op1 & 0xf0) == 0x70)	// jxx rel 8 bit
	       || (op1 == 0xeb)			// jmp rel 8 bit
	       || (op1 == 0xe9)			// jmp rel 16/32 bit
	       || ((op1 == 0x0f)
	           && ((op2 & 0xf0) == 0x80))	// jxx rel 16/32 bit
	       || ((op1 == 0xff)
	           && ((op2 & 0x30) == 0x20))	// jmp/ljmp *(...)
	       || (op1 == 0xea));		// ljmp xxxx:xxxx

  Jdb_ia32_base::code_int.cpu(cpu) =
	      (   (op1 == 0xcc)			// int3
	       || (op1 == 0xcd)			// int xx
	       || (op1 == 0xce));		// into
}
#endif

#ifdef CONFIG_BIT64
static inline void analyze_code(Cpu_number) {}
#endif

bool
Jdb_ia32_base::handle_special_cmds(int c)
{
  Jdb::foreach_cpu(&analyze_code);

  switch (c)
    {
    case 'j': // do restricted "go"
      switch (putchar(c=Kconsole::console()->getchar()))
	{
	case 'b': // go until next branch
	case 'r': // go until current function returns
	  ss_level.cpu(Jdb::triggered_on_cpu) = 0;
	  if (code_call.cpu(Jdb::triggered_on_cpu))
	    {
	      // increase call level because currently we
	      // stay on a call instruction
	      ss_level.cpu(Jdb::triggered_on_cpu)++;
	    }
	  ss_state.cpu(Jdb::triggered_on_cpu) = (c == 'b') ? SS_BRANCH : SS_RETURN;
	  // if we have lbr feature, the processor treats the single
	  // step flag as step on branches instead of step on instruction
	  Cpu::boot_cpu()->btf_enable(true);
	  // fall through
	case 's': // do one single step
          Jdb::entry_frame.cpu(Jdb::triggered_on_cpu)->flags(Jdb::entry_frame.cpu(Jdb::triggered_on_cpu)->flags() | EFLAGS_TF);
          Jdb::hide_statline = false;
	  return 0;
	default:
          Jdb::abort_command();
	  break;
	}
      break;
    default:
      putstr("\b \b");
      // ignore character and get next input
      break;
    }

  return 1;
}

// entered debugger because of single step trap
static inline int
handle_single_step(Cpu_number cpu)
{
  int really_break = 1;

  analyze_code(cpu);

  Cpu const &ccpu = Cpu::cpus.cpu(cpu);
  Jdb::error_buffer.cpu(cpu).clear();

  // special single_step ('j' command): go until branch/return
  if (Jdb_ia32_base::ss_state.cpu(cpu) != Jdb_ia32_base::SS_NONE)
    {
      if (ccpu.lbr_type() != Cpu::Lbr_unsupported)
	{
	  // don't worry, the CPU always knows what she is doing :-)
	}
      else
	{
	  // we have to emulate lbr looking at the code ...
	  switch (Jdb_ia32_base::ss_state.cpu(cpu))
	    {
	    case Jdb_ia32_base::SS_RETURN:
	      // go until function return
	      really_break = 0;
	      if (Jdb_ia32_base::code_call.cpu(cpu))
		{
		  // increase call level
		  Jdb_ia32_base::ss_level.cpu(cpu)++;
		}
	      else if (Jdb_ia32_base::code_ret.cpu(cpu))
		{
		  // decrease call level
		  really_break = (Jdb_ia32_base::ss_level.cpu(cpu)-- == 0);
		}
	      break;
	    case Jdb_ia32_base::SS_BRANCH:
	    default:
	      // go until next branch
	      really_break = (Jdb_ia32_base::code_ret.cpu(cpu) || Jdb_ia32_base::code_call.cpu(cpu) || Jdb_ia32_base::code_bra.cpu(cpu) || Jdb_ia32_base::code_int.cpu(cpu));
	      break;
	    }
	}

      if (really_break)
	{
	  // condition met
	  Jdb_ia32_base::ss_state.cpu(cpu) = Jdb_ia32_base::SS_NONE;
	  Jdb::error_buffer.cpu(cpu).printf("%s", "Branch/Call");
	}
    }
  else // (ss_state == SS_NONE)
    // regular single_step
    Jdb::error_buffer.cpu(cpu).printf("%s", "Singlestep");

  return really_break;
}

// entered debugger due to debug exception
static inline int
handle_trap1(Cpu_number cpu, Jdb_entry_frame *ef)
{
  // FIXME: currently only on boot cpu
  if (cpu != Cpu_number::boot_cpu())
    return 0;

  if (Jdb_ia32_base::bp_test_sstep && Jdb_ia32_base::bp_test_sstep(cpu, ef))
    return handle_single_step(cpu);

  Jdb::error_buffer.cpu(cpu).clear();
  if (Jdb_ia32_base::bp_test_break
      && Jdb_ia32_base::bp_test_break(cpu, ef, &Jdb::error_buffer.cpu(cpu)))
    return 1;

  if (Jdb_ia32_base::bp_test_other
      && Jdb_ia32_base::bp_test_other(cpu, ef, &Jdb::error_buffer.cpu(cpu)))
    return 1;

  return 0;
}

// entered debugger due to software breakpoint
static inline bool
handle_trap3(Cpu_number cpu, Jdb_entry_frame *ef)
{
  Jdb::error_buffer.cpu(cpu).clear();

  if (ef->debug_entry_kernel_str())
    Jdb::error_buffer.cpu(cpu).printf("%s", ef->text());
  else if (ef->debug_entry_user_str())
    Jdb::error_buffer.cpu(cpu).printf("user \"%.*s\"", ef->textlen(), ef->text());

  return true;
}

// entered debugger due to other exception
static inline int
handle_trapX(Cpu_number cpu, Jdb_entry_frame *ef)
{
  Jdb::error_buffer.cpu(cpu).clear();
  Jdb::error_buffer.cpu(cpu).printf("%s", Cpu::exception_string(ef->_trapno));
  if (   ef->_trapno >= 10
      && ef->_trapno <= 14)
    Jdb::error_buffer.cpu(cpu).printf(" (ERR=" L4_PTR_FMT ")", ef->_err);

  return 1;
}

/** Int3 debugger interface. This function is called immediately
 * after entering the kernel debugger.
 * @return true if command was successfully interpreted
 */
bool
Jdb::handle_user_request(Cpu_number cpu)
{
  Jdb_entry_frame *ef = Jdb::entry_frame.cpu(cpu);

  if (ef->debug_ipi())
    return cpu != Cpu_number::boot_cpu();

  if (ef->debug_entry_kernel_sequence())
    return execute_command_ni(ef->text(), ef->textlen());

  return false;
}

void
Jdb::enter_trap_handler(Cpu_number cpu)
{ Cpu::cpus.cpu(cpu).debugctl_disable(); }

void
Jdb::leave_trap_handler(Cpu_number cpu)
{ Cpu::cpus.cpu(cpu).debugctl_enable(); }

bool
Jdb::handle_conditional_breakpoint(Cpu_number cpu, Jdb_entry_frame *e)
{ return e->_trapno == 1 && bp_test_log_only && bp_test_log_only(cpu, e); }

void
Jdb::handle_nested_trap(Jdb_entry_frame *e)
{
  // re-enable interrupts if we need them because they are disabled
  if (Config::getchar_does_hlt_works_ok)
    Proc::sti();

  switch (e->_trapno)
    {
    case 2:
      cursor(Jdb_screen::height(), 1);
      printf("\nNMI occurred\n");
      break;
    case 3:
      cursor(Jdb_screen::height(), 1);
      printf("\nSoftware breakpoint inside jdb at " L4_PTR_FMT "\n",
             e->ip()-1);
      break;
    case 13:
      switch (msr_test)
	{
	case Msr_test_fail_warn:
	  printf(" MSR does not exist or invalid value\n");
	  msr_test = Msr_test_default;
	  msr_fail = 1;
	  break;
	case Msr_test_fail_ignore:
	  msr_test = Msr_test_default;
	  msr_fail = 1;
	  break;
	default:
	  cursor(Jdb_screen::height(), 1);
	  printf("\nGeneral Protection (eip=" L4_PTR_FMT ","
	      " err=" L4_PTR_FMT ", pfa=" L4_PTR_FMT ") -- jdb bug?\n",
	      e->ip(), e->_err, e->_cr2);
	  break;
	}
      break;
    default:
      cursor(Jdb_screen::height(), 1);
      printf("\nInvalid access (trap=%02lx err=" L4_PTR_FMT
	  " pfa=" L4_PTR_FMT " eip=" L4_PTR_FMT ") "
	  "-- jdb bug?\n",
	  e->_trapno, e->_err, e->_cr2, e->ip());
      break;
    }
}

bool
Jdb::handle_debug_traps(Cpu_number cpu)
{
  bool really_break = true;
  auto *ef = entry_frame.cpu(cpu);

  if (ef->_trapno == 1)
    really_break = handle_trap1(cpu, ef);
  else if (ef->_trapno == 3)
    really_break = handle_trap3(cpu, ef);
  else
    really_break = handle_trapX(cpu, ef);

  if (really_break)
    {
      for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
	{
	  if (!Cpu::online(i) || !running.cpu(i))
	    continue;
	  // else S+ mode
	  if (!permanent_single_step.cpu(i))
	    entry_frame.cpu(i)->flags(entry_frame.cpu(i)->flags() & ~EFLAGS_TF);
	}
    }

  return really_break;
}

bool
Jdb_ia32_base::test_checksums()
{ return Boot_info::get_checksum_ro() == Checksum::get_checksum_ro(); }

int
Jdb_ia32_base::get_register(char *reg)
{
  union
  {
    char c[4];
    Unsigned32 v;
  } reg_name;
  int i;
  reg_name.v = 0;

  putchar(reg_name.c[0] = Jdb_screen::Reg_prefix);

  for (i = 1; i < 3; i++)
    {
      int c = Kconsole::console()->getchar();
      if (c == KEY_ESC)
	return false;
      putchar(reg_name.c[i] = c & 0xdf);
      if (c == '8' || c == '9')
	break;
    }

  reg_name.c[3] = '\0';

  for (i = 0; i < Jdb_screen::num_regs(); i++)
    if (reg_name.v == *reinterpret_cast<Unsigned32 const *>(Jdb_screen::Reg_names[i]))
      break;

  if (i == Jdb_screen::num_regs())
    return false;

  *reg = i + 1;
  return true;
}

void
Jdb::write_tsc_s(String_buffer *buf, Signed64 tsc, bool sign)
{
  Unsigned64 uns = Cpu::boot_cpu()->tsc_to_ns(tsc < 0 ? -tsc : tsc);

  if (tsc < 0)
    uns = -uns;

  if (sign)
    buf->printf("%c", (tsc < 0) ? '-' : (tsc == 0) ? ' ' : '+');

  Mword _s  = uns / 1000000000;
  Mword _us = div32(uns, 1000) - 1000000 * _s;
  buf->printf("%3lu.%06lu s ", _s, _us);
}

void
Jdb::write_tsc(String_buffer *buf, Signed64 tsc, bool sign)
{
  Unsigned64 ns = Cpu::boot_cpu()->tsc_to_ns(tsc < 0 ? -tsc : tsc);
  if (tsc < 0)
    ns = -ns;
  write_ll_ns(buf, ns, sign);
}

//----------------------------------------------------------------------------
#ifdef CONFIG_MP

void
Jdb::send_nmi(Cpu_number cpu)
{
  Apic::mp_send_ipi(Apic::Ipi_dest_shrt::Noshrt, Apic::apic.cpu(cpu)->apic_id(),
                    Apic::Ipi_delivery_mode::Nmi, 0);
}

#endif
