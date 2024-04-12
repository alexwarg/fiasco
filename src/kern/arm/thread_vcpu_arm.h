
#pragma once

#include <globalconfig.h>

#ifdef CONFIG_CPU_VIRT

template<typename BASE>
class Thread_vcpu_arm_t : public BASE
{
public:
  static bool ext_vcpu_available()
  { return true; }

  static int pre_check(Context *c, bool ext)
  {
    if (ext && !c->check_for_current_cpu())
      return -L4_err::EInval;

    return 0;
  }

};

#else
template<typename BASE>
class Thread_vcpu_arch_t : public BASE {};
#endif
