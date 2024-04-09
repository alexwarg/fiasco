#pragma once

#include <svm.h>
#include <config.h>
#include <vm.h>

#if defined (CONFIG_JDB)
#include "tb_entry.h"
#include "string_buffer.h"
#endif // CONFIG_JDB

struct Vmcb;
class Svm;

class Vm_svm : public Vm
{
public:

  void *operator new (size_t size, void *p) noexcept
  {
    (void)size;
    assert (size == sizeof (Vm_svm));
    return p;
  }

  void operator delete (void *ptr);

  Vm_svm(Ram_quota *q) : Vm(q)
  {}

  int resume_vcpu(Context *ctxt, Vcpu_state *vcpu, bool user_mode) override;

private:
  static void resume_vm_svm(Mword phys_vmcb, Trex *regs)
    asm("resume_vm_svm") __attribute__((__regparm__(3)));

  enum
  {
    EFER_LME = 1 << 8,
    EFER_LMA = 1 << 10,
  };

  static Vmcb *ext_state(Vcpu_state *s)
  {
    // 0x400: offset into vCPU state page for VMCB start.
    return reinterpret_cast<Vmcb *>(reinterpret_cast<char *>(s) + 0x400);
  }

  void restore_segments(Context *, Unsigned16 fs, Unsigned16 gs);
  void copy_state_save_area(Vmcb *dest, Vmcb *src);
  void copy_control_area(Vmcb *dest, Vmcb *src);
  void copy_control_area_back(Vmcb *dest, Vmcb *src);
  int do_resume_vcpu(Context *ctxt, Vcpu_state *vcpu, Vmcb *vmcb_s);
  Address get_vm_cr3(Vmcb*);

protected:
#if defined (CONFIG_JDB)
  struct Log_vm_svm_exit : public Tb_entry
  {
    Mword exitcode, exitinfo1, exitinfo2, rip;
    void print(String_buffer *buf) const;
  };
#endif // CONFIG_JDB
};


