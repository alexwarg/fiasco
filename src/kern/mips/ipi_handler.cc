
#include <ipi.h>
#include <mips_cpu_irqs.h>
#include <sched.h>
#include <thread.h>
#include <traps.h>

namespace {

class Remote_irq : public Irq_base
{
public:
  // we assume IPIs to be top level, no upstream IRQ chips
  void handle(Upstream_irq const *)
  {
    auto c = current_cpu();
    auto ipi = Ipi::ipis(c);
    Ipi::hw->ack_ipi(c);

    if (ipi->atomic_reset(Ipi::Request))
      Sched<>::handle_remote_requests_irq();

    if (ipi->atomic_reset(Ipi::Global_request))
      Thread::handle_global_remote_requests_irq();

    if (ipi->atomic_reset(Ipi::Debug))
      {
        // fake a trap-state for the nested_trap handler and set the cause to debug ipi
        Trap_state ts;
        Trap_state::Cause *cause = &reinterpret_cast<Trap_state::Cause &>(ts.cause);
        cause->bp_spec() = 3;
        cause->exc_code() = 9;
        call_nested_trap_handler(&ts);
      }
  }

  Remote_irq()
  {
    if (Ipi::hw == nullptr)
      return;

    Ipi::hw->init_ipis(Cpu_number::boot_cpu(), this);
    set_hit(&handler_wrapper<Remote_irq>);
    unmask();
  }

  void switch_mode(bool) override {}
};

static Remote_irq _ipiiii;

}
