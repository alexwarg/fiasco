
#include <types.h>
#include <irq_mgr.h>
#include <per_cpu_data.h>
#include <context.h>
#include <fpu.h>
#include <arm_hyp_irqs.h>

#include <globalconfig.h>

namespace {

inline void
vcpu_vgic_upcall(Context *c, unsigned virq)
{
  assert (c->state.has(Thread_ext_vcpu_enabled));
  assert (c->state.has(Thread_vcpu_user));

  Vcpu_state *vcpu = c->vcpu_state().access();
  assert (c->vcpu_exceptions_enabled(vcpu));

  Trap_state *ts = static_cast<Trap_state *>((Return_frame *)c->regs());

#ifdef CONFIG_FPU
  // Before entering kernel mode to have original fpu state before
  // enabling FPU
  char *esu = (char *)&c->utcb().access()->values[21];
  Fpu::save_user_exception_state(c->state.has(Thread_fpu_owner),
                                 c->fpu_state(),
                                 ts, (Fpu::Exception_state_user *)esu);
#endif
  c->spill_user_state();

  check (c->vcpu_enter_kernel_mode(vcpu));
  vcpu = c->vcpu_state().access();

  vcpu->_regs.s.esr.ec() = 0x3d;
  vcpu->_regs.s.esr.svc_imm() = virq;

  c->vcpu_save_state_and_upcall();
}


class Arm_ppi_virt : public Irq_base
{
public:

  Arm_ppi_virt(unsigned irq, unsigned virq) : _virq(virq), _irq(irq)
  {
    set_hit(handler_wrapper<Arm_ppi_virt>);
  }

  void alloc(Cpu_number cpu)
  {
    check (Irq_mgr::mgr->alloc(this, _irq, false));
    chip()->set_mode(pin(), Irq_chip::Mode::F_level_high);
    chip()->unmask_percpu(cpu, pin());
  }

  [[gnu::flatten]]
  void handle(Upstream_irq const *ui)
  {
    vcpu_vgic_upcall(current(), _virq);
    chip()->ack(pin());
    Upstream_irq::ack(ui);
  }

private:
  void switch_mode(bool) override {}

  unsigned _virq;
  unsigned _irq;
};

class Arm_vtimer_ppi : public Irq_base
{
public:
  Arm_vtimer_ppi(unsigned irq) : _irq(irq)
  {
    set_hit(handler_wrapper<Arm_vtimer_ppi>);
  }

  void alloc(Cpu_number cpu)
  {
    printf("Allocate ARM PPI %d to virtual %d\n", _irq, 1);
    check (Irq_mgr::mgr->alloc(this, _irq, false));
    chip()->set_mode(pin(), Irq_chip::Mode::F_level_high);
    chip()->unmask_percpu(cpu, pin());
  }

  [[gnu::flatten]]
  void handle(Upstream_irq const *ui)
  {
    mask();
    vcpu_vgic_upcall(current(), 1);
    chip()->ack(pin());
    Upstream_irq::ack(ui);
  }

private:
#ifdef CONFIG_BIT32
  void mask()
  {
    Mword v;
    asm volatile("mrc p15, 0, %0, c14, c3, 1\n"
                 "orr %0, #0x2              \n"
                 "mcr p15, 0, %0, c14, c3, 1\n" : "=r" (v));
  }
#endif
#ifdef CONFIG_BIT64
  void mask()
  {
    Mword v;
    asm volatile("mrs %0, cntv_ctl_el0\n"
                 "orr %0, %0, #0x2              \n"
                 "msr cntv_ctl_el0, %0\n" : "=r" (v));
  }
#endif

  void switch_mode(bool) override {}
  unsigned _irq;
};

static Arm_ppi_virt __vgic_irq(Hyp_irqs<>::vgic, 0);  // virtual GIC
static Arm_vtimer_ppi __vtimer_irq(Hyp_irqs<>::vtimer); // virtual timer

struct Local_irq_init
{
  explicit Local_irq_init(Cpu_number cpu)
  {
    if (cpu >= Cpu::invalid())
      return;

    __vgic_irq.alloc(cpu);
    __vtimer_irq.alloc(cpu);
  }
};

DEFINE_PER_CPU_LATE static Per_cpu<Local_irq_init>
  local_irqs(Per_cpu_data::Cpu_num);

}

