#pragma once

#include <globalconfig.h>

#ifdef CONFIG_CPU_VIRT

#include <cpu.h>
#include <vz.h>

template<typename BASE>
class Thread_vcpu_arch_t : public BASE
{
public:
  static bool ext_vcpu_available()
  { return Cpu::options.vz(); }

  static void init_state(Context *c, Vcpu_state *vcpu_state, bool ext)
  {
    if (!ext || c->state.has(Thread_ext_vcpu_enabled))
      return;

    Vz::State *v = c->vm_state(vcpu_state);
    v->init();
  }
};

#else
template<typename BASE>
class Thread_vcpu_arch_t : public BASE {};
#endif
