#pragma once

#include "l4_types.h"
#include "types.h"
#include "mem_layout.h"

class Syscall_post_frame
{
  Mword    _r15;
  Mword    _r14;
  Mword    _r13;
  Mword    _r12;
  Mword    _r11;
  Mword    _r10;
  Mword     _r9;
  Mword     _r8;
};

class Syscall_frame
{
protected:
  Mword    _rdi;
  Mword    _rsi;
  Mword    _rbp;
  Mword    _reserved;
  Mword    _rbx;
  Mword    _rdx;
  Mword    _rcx;
  Mword    _rax;

public:
  Mword from_spec() const
  { return _rsi; }

  void from(Mword f)
  { _rsi = f; }

  L4_obj_ref ref() const
  { return L4_obj_ref::from_raw(_rdx); }

  void ref(L4_obj_ref const &r)
  { _rdx = r.raw(); }

  L4_timeout_pair timeout() const
  { return L4_timeout_pair(_rcx); }

  void timeout(L4_timeout_pair const &to)
  { _rcx = to.raw(); }

  Utcb *utcb() const
  { return reinterpret_cast<Utcb*>(_rdi); }

  L4_msg_tag tag() const
  { return L4_msg_tag(_rax); }

  void tag(L4_msg_tag const &tag)
  { _rax = tag.raw(); }
};

class Syscall_pre_frame
{
  Mword    _reserved_1[2];
};

class Return_frame
{
private:
  Mword    _rip;
  Mword     _cs;
  Mword _rflags;
  Mword    _rsp;
  Mword     _ss;

public:
  enum { Pf_ax_offset = 2 };

  Address ip() const
  { return _rip; }

  Address ip_syscall_user() const
  { return ip(); }

  void ip(Mword ip);

  Address sp() const
  { return _rsp; }

  void sp(Mword sp)
  { _rsp = sp; }

  Mword flags() const
  { return _rflags; }

  void flags(Mword flags)
  { _rflags = flags; }

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
class Entry_frame
: public Syscall_post_frame,
  public Syscall_frame,
  public Syscall_pre_frame,
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
      /* symbols from the assember entry code */
      extern Mword leave_from_syscall_by_iret;
      Mword **ret_from_disp_syscall = reinterpret_cast<Mword**>(static_cast<Entry_frame*>(this))-1;
      cs(cs() & ~0x80);
      *ret_from_disp_syscall = &leave_from_syscall_by_iret;
    }

 _rip = ip;
}

