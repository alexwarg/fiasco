#pragma once

#include "l4_types.h"
#include "types.h"
#include "mem_layout.h"

class Syscall_frame
{
protected:
  Mword             _ecx;
  Mword             _edx;
  Mword             _esi;
  Mword             _edi;
  Mword             _ebx;
  Mword             _ebp;
  Mword             _eax;

public:
  Mword from_spec() const
  { return _esi; }

  void from(Mword f)
  { _esi = f; }

  L4_obj_ref ref() const
  { return L4_obj_ref::from_raw(_edx); }

  void ref(L4_obj_ref const &ref)
  { _edx = ref.raw(); }

  L4_timeout_pair timeout() const
  { return L4_timeout_pair(_ecx); }

  void timeout(L4_timeout_pair const &to)
  { _ecx = to.raw(); }

  Utcb *utcb() const
  { return reinterpret_cast<Utcb*>(_edi); }

  L4_msg_tag tag() const
  { return L4_msg_tag(_eax); }

  void tag(L4_msg_tag const &tag)
  { _eax = tag.raw(); }
};

class Return_frame
{
private:
  Mword             _eip;
  Unsigned16  _cs, __attribute__((unused)) __csu;
  Mword          _eflags;
  Mword             _esp;
  Unsigned16  _ss, __attribute__((unused)) __ssu;

public:
  enum { Pf_ax_offset = 0 };

  Address ip() const
  { return _eip; }

  void ip(Mword ip);

  Address ip_syscall_page_user() const
  {
    Address eip = ip();
    if ((eip & Mem_layout::Syscalls) == Mem_layout::Syscalls
        && (int)Config::Access_user_mem == Config::Access_user_mem_direct)
       eip = *(Mword *)sp();
    return eip;
  }

  Address sp() const
  { return _esp; }

  void sp(Mword sp)
  { _esp = sp; }

  Mword flags() const
  { return _eflags; }

  void flags(Mword flags)
  { _eflags = flags; }

  Mword cs() const
  { return _cs; }

  void cs(Mword cs)
  { _cs = cs; }

  Mword ss() const
  { return _ss; }

  void ss(Mword ss)
  { _ss = ss; }
};

/**
 * Encapsulation of a syscall entry kernel stack.
 *
 * This class encapsulates the complete top of the 
 * kernel stack after a syscall (including the 
 * iret return frame).
 */
class Entry_frame :
  public Syscall_frame,
  public Return_frame
{
public:
  static Entry_frame *to_entry_frame(Syscall_frame *sf)
  { return nonull_static_cast<Entry_frame *>(sf); }

  Syscall_frame *syscall_frame() { return this; }
  Syscall_frame const *syscall_frame() const { return this; }
} __attribute__((packed));

inline void
Return_frame::ip(Mword ip)
{
  // We have to consider a special case where we have to leave the kernel
  // with iret instead of sysexit: If the target thread entered the kernel
  // through sysenter, it would leave using sysexit. This is not possible
  // for two reasons: Firstly, the sysexit instruction needs special user-
  // land code to load the right value into the edx register (see user-
  // level sysenter bindings). And secondly, the sysexit instruction
  // decrements the user-level eip value by two to ensure that the fixup
  // code is executed. One solution without kernel support would be to add
  // the instructions "movl %ebp, %edx" just _before_ the code the target
  // eip is set to.
  if (cs() & 0x80)
    {
      /* symbols from the assembler entry code */
      extern Mword leave_from_sysenter_by_iret;
      extern Mword leave_alien_from_sysenter_by_iret;
      extern Mword ret_from_fast_alien_ipc;
      Mword **ret_from_disp_syscall = reinterpret_cast<Mword**>(static_cast<Entry_frame*>(this))-1;
      cs(cs() & ~0x80);
      if (*ret_from_disp_syscall == &ret_from_fast_alien_ipc)
        *ret_from_disp_syscall = &leave_alien_from_sysenter_by_iret;
      else
        *ret_from_disp_syscall = &leave_from_sysenter_by_iret;
    }

  _eip = ip;
}

