#pragma once

#include <task.h>
#include <cxx/dyn_cast>
#ifdef CONFIG_JDB
#include <tb_entry.h>
#endif

class Vm : public cxx::Dyn_castable<Vm, Task>
{
public:
  explicit Vm(Ram_quota *q)
    : Dyn_castable_class(q, Caps::mem() | Caps::obj()) {}

  struct Vm_state_mode
  {
    Mword sp;
    Mword lr;
    Mword spsr;
  };

  enum
  {
    Max_num_inject_irqs = 32 * 8,
    Nr_irq_fields = (Max_num_inject_irqs + (sizeof(Unsigned32) * 8 - 1))
                    / (sizeof(Unsigned32) * 8),
  };

  struct Vm_state_irq_inject
  {
    Unsigned32 group;
    Unsigned32 irqs[Nr_irq_fields];
  };

  struct Vm_state
  {
    Mword r[13];

    Mword sp_usr;
    Mword lr_usr;

    Vm_state_mode irq;
    Mword r_fiq[5]; // r8 - r12
    Vm_state_mode fiq;
    Vm_state_mode abt;
    Vm_state_mode und;
    Vm_state_mode svc;

    Mword pc;
    Mword cpsr;

    Mword pending_events;
    Unsigned32 cpacr;
    Mword cp10_fpexc;

    Mword pfs;
    Mword pfa;
    Mword exit_reason;

    Vm_state_irq_inject irq_inject;
  };

  int resume_vcpu(Context *ctxt, Vcpu_state *vcpu, bool user_mode) override;

  bool get_fpu();

  Page_number map_max_address() const
  { return Page_number(1UL << (MWORD_BITS - Mem_space::Page_shift)); }

  void *operator new(size_t size, void *p) noexcept;
  void operator delete(void *ptr);

#ifdef CONFIG_JDB
  struct Vm_log : public Tb_entry
  {
    bool is_entry;
    Mword pc;
    Mword cpsr;
    Mword exit_reason;
    Mword pending_events;
    Mword r0;
    Mword r1;
    void print(String_buffer *buf) const;
  };

  void dump_vm_state();
  void show_short(String_buffer *buf);
  static void log_vm(Vm_state *state, bool is_entry);
#else
  static void log_vm(Vm_state *, bool) {}
#endif

private:
  Vm_state *state_for_dbg;

  enum Exit_reasons
  {
    ER_none       = 0,
    ER_vmm_call   = 1,
    ER_inst_abort = 2,
    ER_data_abort = 3,
    ER_irq        = 4,
    ER_fiq        = 5,
    ER_undef      = 6,
  };

#ifdef CONFIG_JDB
  Mword jdb_get(Mword *state_ptr);
#endif
};


