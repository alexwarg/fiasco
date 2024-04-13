
#include <thread.h>
#include <alternatives.h>
#include <thread_vcpu.h>
#include <traps.h>
#include <handle_pagefault.h>
#include <log_pagefault.h>

#include <globalconfig.h>

extern "C" void sys_kdb_ke();

#ifdef CONFIG_JDB

#include <kernel_task.h>

#include <dbg_stack.h>
#include <kdb_ke.h>
#include <cpu_call.h>

#include <cstring>

Trap_state::Handler Thread::nested_trap_handler FIASCO_FASTCALL;

void sys_kdb_ke()
{
  enum Kernel_entry_op
  {
    Op_kdebug_none = 0,
    Op_kdebug_text = 1,
    Op_kdebug_call = 2,
  };
  cpu_lock.lock();
  char str[32] = "BREAK ENTRY";
  Thread *t = current_thread();
  Entry_frame *regs = t->regs();
  Trap_state *ts = static_cast<Trap_state*>(regs);
  Kernel_entry_op kdb_ke_op = Kernel_entry_op(ts->r[Trap_state::R_s6]);
  Mword arg = ts->r[Trap_state::R_t0];

  switch (kdb_ke_op)
    {
    case Op_kdebug_text:
      strncpy(str, (char *)(arg), sizeof(str));
      str[sizeof(str)-1] = 0;
      break;

    default:
      break;
    }

  kdb_ke(str);
}

inline int
_call_nested_trap_handler(Trap_state *ts)
{
  Cpu_phys_id phys_cpu = Proc::cpu_id();
  Cpu_number log_cpu = Cpu::cpus.find_cpu(Cpu::By_phys_id(phys_cpu));
  if (log_cpu == Cpu_number::nil())
    {
      printf("Trap on unknown CPU phys_id=%x\n",
             cxx::int_value<Cpu_phys_id>(phys_cpu));
      log_cpu = Cpu_number::boot_cpu();
    }

  unsigned long &ntr = Thread::nested_trap_recover.cpu(log_cpu);
  void *stack = 0;

  if (!ntr)
    stack = Dbg::dbg_stack.cpu(log_cpu).stack_top;

#if 0
  Mem_space *m = Mem_space::current_mem_space(log_cpu);
  if (Kernel_task::kernel_task() != m)
    Kernel_task::kernel_task()->make_current();
#endif
  // FIXME: AW what does this do
  // ts->set_errorcode(ts->error() | ts->r[Syscall_frame::REG_T0]);

  Mword dummy1, tmp, ret;
  {
    register Mword _ts asm("$4") = (Mword)ts;      // $4 == a0
    register Mword res asm("$2");                  // $2 == v0
    register Cpu_number _lcpu asm("$5") = log_cpu; // $5 == a1

    asm volatile(
        ".set push                        \n"
        ".set noreorder                   \n"
        " move  %[origstack], $sp         \n"
        " " ASM_L " %[tmp], 0(%[ntr])     \n"
        " bnez  %[tmp], 1f                \n"
        " nop                             \n"
        " move  $sp, %[stack]             \n"
        "1:                               \n"
        " " ASM_ADDIU " %[tmp], %[tmp], 1 \n"
        " " ASM_S " %[tmp], 0(%[ntr])     \n"
        " " ASM_ADDIU " $sp, $sp, -(%[frsz] + 2 * %[rsz]) \n" //set up call frame
        " " ASM_S " %[origstack], (%[rsz] + %[frsz])($sp) \n"
        " " ASM_S " %[ntr], (%[frsz])($sp)                \n"
        " jalr  %[handler]                \n"
        " nop                             \n"
        " " ASM_L " %[ntr], (%[frsz])($sp)\n"
        " " ASM_L " $sp, (%[rsz] + %[frsz])($sp)          \n"
        " " ASM_L " %[tmp], 0(%[ntr])     \n"
        " " ASM_ADDIU " %[tmp], %[tmp], -1\n"
        " " ASM_S " %[tmp], 0(%[ntr])     \n"
        ".set pop                         \n"
        : [origstack] "=&r" (dummy1), [tmp] "=&r" (tmp),
          "+r" (_ts), "+r" (_lcpu), "+r" (res)
        : [ntr] "r" (&ntr), [stack] "r" (stack),
          [handler] "r" (Thread::nested_trap_handler),
          [rsz] "n" (sizeof(Mword)),
          [frsz] "n" (ASM_NARGSAVE * sizeof(Mword))
        : "memory", "$3", "$6", "$7", "$8", "$9", "$10", "$11",
           "$12", "$13", "$14", "$15", "$24", "$25", "$31");

    ret = res;
    ts->epc +=4;
  }

  // the jdb-cpu might have changed things we shouldn't miss!
  // FIXME: MIPS CACHE
  //Mmu<Mem_layout::Cache_flush_area, true>::flush_cache();
#if 0
  if (m != Kernel_task::kernel_task())
    m->make_current();
#endif
  if (!ntr)
    Cpu_call::handle_global_requests();

  return ret;
}
#else // CONFIG_JDB
inline int
call_nested_trap_handler(Trap_state *)
{ return -1; }

void sys_kdb_ke()
{}
#endif // CONFIG_JDB

inline void
save_bad_instr(Trap_state *ts)
{
  asm volatile (ALTERNATIVE_INSN(
        "move %0, $0",
        "mfc0 %0, $8, 1",
        0x8 /* FEATURE_BI */)
      : "=r"(ts->bad_instr));
  asm volatile (ALTERNATIVE_INSN(
        "move %0, $0",
        "mfc0 %0, $8, 2",
        0x10 /* FEATURE_BP */)
      : "=r"(ts->bad_instr_p));
}

[[gnu::flatten]]
int call_nested_trap_handler(Trap_state *ts)
{ return _call_nested_trap_handler(ts); }

inline int
handle_slow_trap(Thread *c, Trap_state::Cause cause, Trap_state *ts,
                 bool save_bad = true)
{
  bool const from_user = ts->status & Trap_state::S_ksu;

  if (save_bad && from_user)
    save_bad_instr(ts);

  if (from_user && c->space_ref()->user_mode() && c->send_exception(ts))
    return 0;

  if (EXPECT_FALSE(!from_user))
    return _call_nested_trap_handler(ts);

  // FIXME: HACK trap 'break' to JDB even from user mode
  if (cause.exc_code() == 9)
    return _call_nested_trap_handler(ts);

  if (c->send_exception(ts))
    return 0;

  // FIXME: should probably kill the thread and schedule here,
  //        enter JDB for now
  return _call_nested_trap_handler(ts);
}


[[gnu::flatten]]
void thread_handle_trap(Mword cause, Trap_state *ts)
{
  Thread *ct = current_thread();
  LOG_TRAP_CN(ct, cause);
  if (handle_slow_trap(ct, cause, ts))
    ct->kill();
}

[[gnu::flatten]]
void handle_fpu_trap(Trap_state::Cause cause, Trap_state *ts)
{
  Thread *ct = current_thread();
  LOG_TRAP_CN(ct, cause.raw);
  if (!ct->switchin_fpu() && handle_slow_trap(ct, cause, ts))
    ct->kill();
}

inline
int
user_page_fault(Thread *t, Mword cause, Trap_state *ts, Mword pfa)
{
  if (Thread_vcpu::vcpu_pagefault(t, pfa, cause, ts->epc))
    return 1;

  Log::page_fault(pfa, cause, ts->epc);

  if (Mem_layout::in_kernel(pfa))
    return 0;

  Proc::sti();
  return handle_user_space_page_fault(t, pfa, cause);
}

inline
int
kern_page_fault(Thread *t, Mword cause, Trap_state *ts, Mword pfa)
{
  Log::page_fault(pfa, cause, ts->epc);
  // Check for page fault in user memory area
  if (!Mem_layout::in_kernel(pfa))
    {
      Proc::sti();
      return handle_user_space_page_fault(t, pfa, cause);
    }

  WARN("No page-fault handler for 0x%lx, cause 0x%lx, pc " L4_PTR_FMT "\n",
        pfa, cause, ts->epc);

  t->do_recover_jmp_buf();
  return 0;
}

[[gnu::flatten]]
void thread_handle_tlb_fault(Mword cause, Trap_state *ts, Mword pfa)
{
  Thread *t = current_thread();
  Space *s = t->vcpu_aware_space();
  LOG_TRACE("TLB miss", "tlb", t, Tb_entry_pf,
      l->set(t, ts->epc, pfa, cause, s);
  );

  assert (s);
  // Uses reserved Cause bit 0 (see exception.S) to flag a TLB miss
  bool need_probe = !(cause & 1);
  bool guest = ts->status & (1 << 3);

  if (EXPECT_FALSE(PF::is_tlb_rights_error(cause)
                   || !s->add_tlb_entry(Virt_addr(pfa),
                                        !PF::is_read_error(cause), need_probe,
                                        guest)))
    {
      // TODO: Think about t->state_del(Thread_cancel); and sync with
      // at least ARM
      save_bad_instr(ts);
      if (Thread_vcpu::vcpu_pagefault(t, pfa, cause, ts->epc))
        return;

      int res;
      if (PF::is_usermode_error(cause))
        res = user_page_fault(t, cause, ts, pfa);
      else
        res = kern_page_fault(t, cause, ts, pfa);

      if (!res && handle_slow_trap(t, cause, ts, false))
        {
          t->kill();
          return;
        }

      s->add_tlb_entry(Virt_addr(pfa), !PF::is_read_error(cause), true, guest);
    }
}

// handle exceptions that MUST usually never happen
[[gnu::flatten]]
void thread_unhandled_trap(Mword, Trap_state *ts)
{
  if (_call_nested_trap_handler(ts))
    current_thread()->kill();
}

#ifdef CONFIG_CPU_VIRT

static bool
thread_guest_tlb_probe(Mword *pfa)
{
  using namespace Mips;
  Mword gindex;
  Mword gentryhi;
  Mword gctl1 = Mem_unit::vz_guest_ctl1();
  Mem_unit::set_vz_guest_rid(gctl1, gctl1 & 0xff);
  mfgc0_32(&gindex, Cp0_index);
  mfgc0(&gentryhi, Cp0_entry_hi);
  mtgc0((*pfa & ~0x3ff) | (gentryhi & 0x3ff), Cp0_entry_hi);
  asm volatile (".set push; .set virt; ehb; tlbgp; ehb; .set pop");
  Mword index;
  mfgc0_32(&index, Cp0_index);
  if (index & (1UL << 31))
    {
      mtgc0_32(gindex, Cp0_index);
      mtgc0(gentryhi, Cp0_entry_hi);
      ehb();

      return false;
    }

  Mword gelo[2];
  Mword gpmask;
  mfgc0(&gelo[0], Cp0_entry_lo1);
  mfgc0(&gelo[1], Cp0_entry_lo2);
  mfgc0_32(&gpmask, Cp0_page_mask);
  asm volatile (".set push; .set virt; tlbgr; ehb; .set pop");

  Mword entry;
  Mword mask;
  mfgc0_32(&mask, Cp0_page_mask);
  mask = ((mask | 0x1fff) + 1) >> 1;
  if (*pfa & mask)
    mfgc0(&entry, Cp0_entry_lo2);
  else
    mfgc0(&entry, Cp0_entry_lo1);

  *pfa = ((entry & 0x3fffffc0) << 6) | (*pfa & 0xfff);

  mtgc0(gelo[0], Cp0_entry_lo1);
  mtgc0(gelo[1], Cp0_entry_lo2);
  mtgc0_32(gpmask, Cp0_page_mask);
  mtgc0_32(gindex, Cp0_index);
  mtgc0(gentryhi, Cp0_entry_hi);
  ehb();
  return true;
}

inline bool
thread_translate_gva_32bit_segments(Mword *pfa)
{
  Mword cfg;
#define ASM_SEGCTL_ALT(val, reg, feature) \
  asm (ALTERNATIVE_INSN(                  \
        "li %0, "#val,                    \
        ".set push; .set virt\n\t"        \
        "mfgc0 %0, $5, "#reg"\n\t"        \
        ".set pop", (1 << 8)) : "=r" (cfg))


  if (*pfa & (1UL << 31))
    {
      if (*pfa & (1UL << 30))
        ASM_SEGCTL_ALT(0x00200010, 2, 1 << 8);
      else
        ASM_SEGCTL_ALT(0x00030002, 3, 1 << 8);

      if (*pfa & (1UL << 29))
        cfg &= 0xffff;
      else
        cfg = (cfg >> 16) & 0xffff;
    }
  else
    {
      ASM_SEGCTL_ALT(0x04330033, 4, 1 << 8);

      if (*pfa & (1UL << 30))
        cfg &= 0xffff;
      else
        cfg = (cfg >> 16) & 0xffff;
    }

#undef ASM_SEGCTL_ALT

  unsigned am = (cfg >> 4) & 0x7;
  bool is_mapped = false;
  switch (am)
    {
    case 0: /*UK*/    break;
    case 1: /*MK*/    is_mapped = true; break;
    case 2: /*MSK*/   is_mapped = true; break;
    case 3: /*MUSK*/  is_mapped = true; break;
    case 4: /*MUSUK*/
      {
        Mword gstatus = mfgc0_32(Mips::Cp0_status);
        unsigned plevel = (gstatus >> 3) & 0x3;
        if (gstatus & 0x6)
          plevel = 0;
        is_mapped = (plevel != 0);
        break;
      }
    case 5: /*USK*/  break;
    case 7: /*UUSK*/ break;
    default: is_mapped = true; break;
    }

  if (!is_mapped)
    {
      if (*pfa & (1UL << 31))
        *pfa = (*pfa & 0x1fffffff) | ((cfg >> 9) << 29);
      else
        *pfa = (*pfa & 0x3fffffff) | ((cfg >> 10) << 30);
    }

  return is_mapped;
}

#ifdef CONFIG_BIT32
[[gnu::flatten]]
inline bool
thread_translate_gva(Mword *pfa)
{
  return thread_translate_gva_32bit_segments(pfa);
}

#endif // CONFIG_BIT32
#ifdef CONFIG_BIT64

[[gnu::flatten]]
inline bool
thread_translate_gva(Mword *pfa)
{
  if (*pfa < (1UL << 31) || *pfa >= ~((1UL << 31) - 1))
    return thread_translate_gva_32bit_segments(pfa);

  if (*pfa >> 62 == 2) // xkphys
    {
      *pfa &= (1UL << 59) -1;
      return false;
    }

  return true;
}

#endif // CONFIG_BIT64

[[gnu::flatten]]
static void
thread_handle_gva_tlb_fault(Mword cause, Trap_state *ts, Mword pfa)
{
  auto *t = current_thread();

  bool is_mapped = thread_translate_gva(&pfa);

  if (is_mapped && !thread_guest_tlb_probe(&pfa))
    {
      if (handle_slow_trap(t, cause, ts, true))
        t->kill();
      return;
    }

  Space *s = t->vcpu_aware_space();
  if (EXPECT_TRUE(s->add_tlb_entry(Virt_addr(pfa), !PF::is_read_error(cause), true, true)))
    return;

  save_bad_instr(ts);

  Vcpu_state *vcpu = t->vcpu_state().access();
  t->spill_user_state();
  t->vcpu_enter_kernel_mode(vcpu);
  vcpu->_regs.s = *ts;
  vcpu->_regs.s.bad_v_addr = pfa;
  Context::vm_state(vcpu)->ctl_0 = (Context::vm_state(vcpu)->ctl_0 & ~0x3c) | (10 << 2);
  Entry::vcpu_return_to_kernel(t, vcpu->_entry_ip, vcpu->_sp, t->vcpu_state().usr().get());
}

extern "C" FIASCO_FASTCALL
void
thread_handle_tlb_fault_vz(Mword cause, Trap_state *ts, Mword pfa);

[[gnu::flatten]]
void thread_handle_tlb_fault_vz(Mword cause, Trap_state *ts, Mword pfa)
{
  if (ts->status & (1 << 3))
    {
      Mword c = Mips::mfc0_32(Mips::Cp0_guest_ctl_0);
      if (EXPECT_FALSE(((c >> 2) & 0x1f) == 8))
        {
          thread_handle_gva_tlb_fault(cause, ts, pfa);
          return;
        }
    }
  thread_handle_tlb_fault(cause, ts, pfa);
}

#endif // CONFIG_CPU_VIRT

