
#include "vm.h"

#include "cpu_lock.h"
#include "entry_frame.h"
#include "ipc_timeout.h"
#include "logdefs.h"
#include "mem_space.h"
#include "thread_state.h"
#include "timer.h"
#include "kmem_slab.h"
#include "gic.h"
#include "thread.h"
#include <entry.h>
#include <task_factory_impl.h>
#include <arm/32/inline_asm.h>

static void tz_switch_to_ns(Mword *nonsecure_state)
{
  extern char go_nonsecure[];

  register void *r0 asm("r0") = nonsecure_state;
  register void *r1 asm("r1") = go_nonsecure;

  asm volatile("push   {" FIASCO_ARM_FPTR_REG "}\n"
               "stmdb sp!, {r0}   \n"
               "mov    r2, sp     \n" // copy sp_svc to sp_mon
               "cps    #0x16      \n" // switch to monitor mode
               "mov    sp, r2     \n"
               "mrs    r4, cpsr   \n" // save return psr
               "adr    r3, " FIASCO_ARM_JMP_LABEL(1f) "\n" // save return eip
               "bx     r1         \n" // go nonsecure!
               "1:                \n"
               "mov    r0, sp     \n" // copy sp_mon to sp_svc
               "cps    #0x13      \n" // switch to svc mode
               "mov    sp, r0     \n"
               "ldmia  sp!, {r0}  \n"
               "pop    {" FIASCO_ARM_FPTR_REG "}\n"
               : : "r" (r0), "r" (r1)
               : FIASCO_ARM_CLOBBER_xFPTR,
                 "r2", "r3", "r4", "r5", "r6",
                 "r8", "r9", "r10", "r12", "r14", "memory");
}



JDB_DEFINE_TYPENAME(Vm, "\033[33;1mVm\033[m");

void *
Vm::operator new([[maybe_unused]] size_t size, void *p) noexcept
{
  assert (size == sizeof(Vm));
  return p;
}

void
Vm::operator delete(void *ptr)
{
  Vm *t = static_cast<Vm *>(ptr);
  Kmem_slab_t<Vm>::q_free(t->ram_quota(), ptr);
}

int
Vm::resume_vcpu(Context *ctxt, Vcpu_state *vcpu, [[maybe_unused]] bool user_mode)
{
  assert(user_mode);

  assert(cpu_lock.test());

  if (EXPECT_FALSE(!ctxt->state.has(Thread_ext_vcpu_enabled)))
    {
      ctxt->arch_load_vcpu_kern_state(vcpu, true);
      return -L4_err::EInval;
    }

  Vm_state *state = offset_cast<Vm_state *>(vcpu, Config::Ext_vcpu_state_offset);

  state_for_dbg = state;

  if (state->irq_inject.group)
    {
      Unsigned32 g = state->irq_inject.group;
      for (unsigned i = 1; i < Nr_irq_fields; ++i)
        if ((1 << i) & g)
          {
            Gic::primary->set_pending_irq(i, state->irq_inject.irqs[i]);
            state->irq_inject.irqs[i] = 0;
          }

      state->irq_inject.group = 0;
    }

  while (true)
    {
      if (   !vcpu->saved_irqs_enabled()
          && vcpu->pending_irqs())
        {
          state->exit_reason = ER_irq;
          ctxt->arch_load_vcpu_kern_state(vcpu, true);
          return 1;
        }

      log_vm(state, 1);

      if (!get_fpu())
        {
          ctxt->arch_load_vcpu_kern_state(vcpu, true);
          return -L4_err::EInval;
        }

      tz_switch_to_ns(reinterpret_cast<Mword *>(state));

      assert(cpu_lock.test());

      log_vm(state, 0);

      if (state->exit_reason == ER_irq || state->exit_reason == ER_fiq)
        Proc::preemption_point();

      switch (state->exit_reason)
        {
        case ER_data_abort:
          if ((state->pfs & 0x237) == 0x211)
            break;
          ctxt->arch_load_vcpu_kern_state(vcpu, true);
          return 0;
        case ER_undef:
          printf("should not happen: %lx\n", state->pc);
          // fall through
        case ER_vmm_call:
        case ER_inst_abort:
          ctxt->arch_load_vcpu_kern_state(vcpu, true);
          return 0;
        }

      Thread *t = nonull_static_cast<Thread*>(ctxt);
      if (t->continuation_test_and_restore())
        {
          ctxt->arch_load_vcpu_kern_state(vcpu, true);
          ::Entry::vcpu_return_to_kernel(t, vcpu->_entry_ip, vcpu->_entry_sp,
                                         t->vcpu_state().usr().get());
        }
    }
}

namespace {

static Kobject_iface * FIASCO_FLATTEN
vm_factory(Ram_quota *q, Space *,
           L4_msg_tag t, Utcb const *u,
           int *err)
{
  return Task::create<Vm>(q, t, u, err);
}

static inline
void __attribute__((constructor)) FIASCO_INIT_SFX(vm_register_factory)
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_vm, vm_factory);
}

}

#ifdef CONFIG_FPU

bool
Vm::get_fpu()
{
  if (!(current()->state() & Thread_fpu_owner))
    {
      if (!current_thread()->switchin_fpu())
        {
          printf("tz: switchin_fpu failed\n");
          return false;
        }
    }
  return true;
}

#else

bool
Vm::get_fpu()
{ return true; }

#endif // CONFIG_FPU

#ifdef CONFIG_JDB

#include "jdb.h"
#include "string_buffer.h"

Mword
Vm::jdb_get(Mword *state_ptr)
{
  Mword v = ~0UL;
  Jdb::peek(Jdb_addr<Mword>(state_ptr, this), v);
  return v;
}

void
Vm::dump_vm_state()
{
  Vm_state *s = state_for_dbg;
  printf("pc: %08lx  cpsr: %08lx exit_reason:%ld \n",
         jdb_get(&s->pc), jdb_get(&s->cpsr), jdb_get(&s->exit_reason));
  printf("r0: %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
         jdb_get(&s->r[0]), jdb_get(&s->r[1]), jdb_get(&s->r[2]), jdb_get(&s->r[3]),
         jdb_get(&s->r[4]), jdb_get(&s->r[5]), jdb_get(&s->r[6]), jdb_get(&s->r[7]));
  printf("r8: %08lx %08lx %08lx %08lx %08lx\n",
         jdb_get(&s->r[8]), jdb_get(&s->r[9]), jdb_get(&s->r[10]), jdb_get(&s->r[11]),
         jdb_get(&s->r[12]));

  printf("usr: sp %08lx lr %08lx\n",
         jdb_get(&s->sp_usr), jdb_get(&s->lr_usr));
  printf("irq: sp %08lx lr %08lx psr %08lx\n",
         jdb_get(&s->irq.sp), jdb_get(&s->irq.lr), jdb_get(&s->irq.spsr));
  printf("fiq: sp %08lx lr %08lx psr %08lx\n",
         jdb_get(&s->fiq.sp), jdb_get(&s->fiq.lr), jdb_get(&s->fiq.spsr));
  printf("r8: %08lx %08lx %08lx %08lx %08lx\n",
         jdb_get(&s->r_fiq[0]), jdb_get(&s->r_fiq[1]), jdb_get(&s->r_fiq[2]),
         jdb_get(&s->r_fiq[3]), jdb_get(&s->r_fiq[4]));

  printf("abt: sp %08lx lr %08lx psr %08lx\n",
         jdb_get(&s->abt.sp), jdb_get(&s->abt.lr), jdb_get(&s->abt.spsr));
  printf("und: sp %08lx lr %08lx psr %08lx\n",
         jdb_get(&s->und.sp), jdb_get(&s->und.lr), jdb_get(&s->und.spsr));
  printf("svc: sp %08lx lr %08lx psr %08lx\n",
         jdb_get(&s->svc.sp), jdb_get(&s->svc.lr), jdb_get(&s->svc.spsr));
}

void
Vm::show_short(String_buffer *buf)
{
  buf->printf(" utcb:%lx pc:%lx ", reinterpret_cast<Mword>(state_for_dbg), reinterpret_cast<Mword>(jdb_get(&state_for_dbg->pc)));
}

void
Vm::Vm_log::print(String_buffer *buf) const
{
  buf->printf("%s: pc:%08lx/%03lx psr:%lx er:%lx r0:%lx r1:%lx",
              is_entry ? "entry" : "exit ",
              pc, pending_events, cpsr, exit_reason, r0, r1);
}

void
Vm::log_vm(Vm_state *state, bool is_entry)
{
  LOG_TRACE("VM entry/entry", "VM", current(), Vm_log,
      l->is_entry = is_entry;
      l->pc = state->pc;
      l->cpsr = state->cpsr;
      l->exit_reason = state->exit_reason;
      l->pending_events = state->pending_events;
      l->r0 = state->r[0];
      l->r1 = state->r[1];
  );
}

#endif // CONFIG_JDB

