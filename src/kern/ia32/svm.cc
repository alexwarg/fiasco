
#include <svm.h>

#include <cpu.h>
#include <kmem.h>
#include <l4_types.h>
#include <warn.h>
#include <kmem_alloc.h>
#include <cstring>

DEFINE_PER_CPU_LATE Per_cpu<Svm> Svm::cpus(Per_cpu_data::Cpu_num);

void
Svm::pm_on_suspend(Cpu_number)
{
  // FIXME: Handle VMCB caching stuff if enabled
}

void
Svm::pm_on_resume(Cpu_number)
{
  Unsigned64 efer = Cpu::rdmsr(MSR_EFER);
  efer |= 1 << 12;
  Cpu::wrmsr(efer, MSR_EFER);
  Unsigned64 vm_hsave_pa = Kmem::virt_to_phys(_vm_hsave_area);
  Cpu::wrmsr(vm_hsave_pa, MSR_VM_HSAVE_PA);
  _last_user_vmcb = 0;
}


bool
Svm::cpu_svm_available(Cpu_number cpu)
{
  Cpu &c = Cpu::cpus.cpu(cpu);

  if (!c.online() || !c.svm())
    return false;

  Unsigned64 vmcr;
  vmcr = c.rdmsr(MSR_VM_CR);
  if (vmcr & (1 << 4)) // VM_CR.SVMDIS
    return false;
  return true;
}

Svm::Svm(Cpu_number cpu)
{
  Cpu &c = Cpu::cpus.cpu(cpu);
  _last_user_vmcb = 0;
  _svm_enabled = false;
  _max_asid = 0;
  _has_npt = false;

  if (!cpu_svm_available(cpu))
    return;

  Unsigned64 efer;
  efer = c.rdmsr(MSR_EFER);
  efer |= 1 << 12;
  c.wrmsr(efer, MSR_EFER);

  Unsigned32 eax, ebx, ecx, edx;
  c.cpuid(0x8000000a, &eax, &ebx, &ecx, &edx);
  if (edx & 1)
    _has_npt = true;

  printf("CPU%u: SVM enabled, nested paging %ssupported, NASID: %u.\n",
         cxx::int_value<Cpu_number>(cpu), _has_npt ? "" : "not ", ebx);
  _max_asid = ebx - 1;

  // FIXME: MUST NOT PANIC ON CPU HOTPLUG
  //assert(_max_asid > 0);

  enum
  {
    Vmcb_size   = 0x1000,
    Io_pm_size  = 0x3000,
    Msr_pm_size = 0x2000,
    State_save_area_size = 0x1000,
  };

  /* 16 KiB IO permission map and Vmcb (16 KiB are good for the buddy allocator)*/
  // FIXME: MUST NOT PANIC ON CPU HOTPLUG
  check(_iopm = Kmem_alloc::allocator()->alloc(Bytes(Io_pm_size + Vmcb_size)));
  _iopm_base_pa = Kmem::virt_to_phys(_iopm);
  _kernel_vmcb = (Vmcb*)((char*)_iopm + Io_pm_size);
  _kernel_vmcb_pa = Kmem::virt_to_phys(_kernel_vmcb);
  _svm_enabled = true;

  /* disbale all ports */
  memset(_iopm, ~0, Io_pm_size);

  /* clean out vmcb */
  memset(_kernel_vmcb, 0, Vmcb_size);

  /* 8 KiB MSR permission map */
  // FIXME: MUST NOT PANIC ON CPU HOTPLUG
  check(_msrpm = Kmem_alloc::allocator()->alloc(Bytes(Msr_pm_size)));
  _msrpm_base_pa = Kmem::virt_to_phys(_msrpm);
  memset(_msrpm, ~0, Msr_pm_size);

  // allow the sysenter MSRs for the guests
  set_msr_perm(MSR_SYSENTER_CS, Msr_rw);
  set_msr_perm(MSR_SYSENTER_EIP, Msr_rw);
  set_msr_perm(MSR_SYSENTER_ESP, Msr_rw);
  set_msr_perm(MSR_GS_BASE, Msr_rw);
  set_msr_perm(MSR_FS_BASE, Msr_rw);
  set_msr_perm(MSR_KERNEL_GS_BASE, Msr_rw);
  set_msr_perm(MSR_STAR, Msr_rw);
  set_msr_perm(MSR_CSTAR, Msr_rw);
  set_msr_perm(MSR_LSTAR, Msr_rw);
  set_msr_perm(MSR_SFMASK, Msr_rw);

  /* 4 KiB Host state-safe area */
  // FIXME: MUST NOT PANIC ON CPU HOTPLUG
  check(_vm_hsave_area = Kmem_alloc::allocator()->alloc(Bytes(State_save_area_size)));
  Unsigned64 vm_hsave_pa = Kmem::virt_to_phys(_vm_hsave_area);

  c.wrmsr(vm_hsave_pa, MSR_VM_HSAVE_PA);
  register_pm(cpu);
}

void
Svm::set_msr_perm(Unsigned32 msr, Msr_perms perms)
{
  unsigned offs;
  if (msr <= 0x1fff)
    offs = 0;
  else if (0xc0000000 <= msr && msr <= 0xc0001fff)
    offs = 0x800;
  else if (0xc0010000 <= msr && msr <= 0xc0011fff)
    offs = 0x1000;
  else
    {
      WARN("Illegal MSR %x\n", msr);
      return;
    }

  msr &= 0x1fff;
  offs += msr / 4;

  unsigned char *pm = (unsigned char *)_msrpm;

  unsigned shift = (msr & 3) * 2;
  pm[offs] = (pm[offs] & ~(3 << shift)) | ((unsigned char)perms << shift);
}

