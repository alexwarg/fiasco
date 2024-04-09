#pragma once

#include <per_cpu_data.h>
#include <virt_ia32_svm.h>
#include <cpu_lock.h>
#include <pm.h>
#include <svm_bits.h>

class Svm : public Pm_object
{
public:
  static Per_cpu<Svm> cpus;

  enum { Gpregs_words = Svm_bits::Gpregs_words };

  enum Msr_perms
  {
    Msr_intercept = 3,
    Msr_ro        = 2,
    Msr_wo        = 1,
    Msr_rw        = 0,
  };

  static bool cpu_svm_available(Cpu_number cpu);

  Svm(Cpu_number cpu);

  void pm_on_suspend(Cpu_number) override;
  void pm_on_resume(Cpu_number) override;

  void set_msr_perm(Unsigned32 msr, Msr_perms perms);

  Unsigned64 iopm_base_pa() const
  { return _iopm_base_pa; }

  Unsigned64 msrpm_base_pa() const
  { return _msrpm_base_pa; }

  Vmcb *kernel_vmcb(Vmcb const *user_vmcb)
  {
    if (user_vmcb != _last_user_vmcb)
      {
        _kernel_vmcb->control_area.clean_bits.raw = 0;
        _last_user_vmcb = user_vmcb;
      }
    else
      _kernel_vmcb->control_area.clean_bits = access_once(&user_vmcb->control_area.clean_bits);

    return _kernel_vmcb;
  }

  Address kernel_vmcb_pa() const
  { return _kernel_vmcb_pa; }

  bool svm_enabled() const
  { return _svm_enabled; }

  bool has_npt() const
  { return _has_npt; }

private:
  Vmcb const *_last_user_vmcb;

  /* read mostly below */
  Unsigned32 _max_asid;
  bool _svm_enabled;
  bool _has_npt;
  void *_vm_hsave_area;
  void *_iopm;
  void *_msrpm;
  Unsigned64 _iopm_base_pa;
  Unsigned64 _msrpm_base_pa;
  Vmcb *_kernel_vmcb;
  Address _kernel_vmcb_pa;
};

