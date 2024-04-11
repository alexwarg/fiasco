#pragma once

class Vcpu_state;

class Context_vcpu_arch_base
{
public:
  void arch_load_vcpu_kern_state(Vcpu_state *, bool)
  {}

protected:
  void arch_load_vcpu_user_state(Vcpu_state *, bool)
  {}

  void arch_vcpu_ext_shutdown()
  {}
};
