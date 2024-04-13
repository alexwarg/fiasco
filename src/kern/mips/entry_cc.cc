#include <thread.h>

extern "C" [[noreturn]]
void leave_by_vcpu_upcall();

[[noreturn]]
void leave_by_vcpu_upcall()
{
  Thread *c = current_thread();
  c->regs()->r[0] = 0; // reset continuation
  Vcpu_state *vcpu = c->vcpu_state().access();
  vcpu->_regs.s = *nonull_static_cast<Trap_state*>(c->regs());
  c->vcpu_return_to_kernel(vcpu->_entry_ip, vcpu->_entry_sp, c->vcpu_state().usr().get());
}


