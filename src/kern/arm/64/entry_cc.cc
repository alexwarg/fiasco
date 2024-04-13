#include <thread.h>
#include <entry.h>

extern "C" [[noreturn]]
void leave_by_vcpu_upcall(Trap_state *ts);

[[noreturn]]
void leave_by_vcpu_upcall(Trap_state *ts)
{
  Thread *c = current_thread();
  Vcpu_state *vcpu = c->vcpu_state().access();
  Mem::memcpy_mwords(vcpu->_regs.s.r, ts->r, 31);
  vcpu->_regs.s.usp = ts->usp;
  vcpu->_regs.s.pc = ts->pc;
  vcpu->_regs.s.pstate = ts->pstate;
  Entry::vcpu_return_to_kernel(c, vcpu->_entry_ip, vcpu->_sp, c->vcpu_state().usr().get());
}

