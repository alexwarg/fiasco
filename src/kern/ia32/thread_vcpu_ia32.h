#pragma once

#include <globalconfig.h>

#ifdef CONFIG_CPU_VIRT

#include <vmx.h>
#include <svm.h>

template<typename BASE>
class Thread_vcpu_ia32_t : public BASE
{
public:
  static bool ext_vcpu_available()
  {
    return Vmx::cpus.current().vmx_enabled() || Svm::cpus.current().svm_enabled();
  }

  static void
  init_state(Context *, Vcpu_state *v, bool ext)
  {
    if (!ext)
      return;

    if (Vmx::cpus.current().vmx_enabled())
      Vmx::cpus.current().init_vmcs_infos(v);

    if (Cpu::boot_cpu()->vendor() == Cpu::Vendor_intel)
      v->user_data[6] = (Mword)Cpu::ucode_revision();
  }
};

#else  // CONFIG_CPU_VIRT

template<typename BASE>
class Thread_vcpu_ia32_t : public BASE
{
};
#endif // CONFIG_CPU_VIRT
