#pragma once

#include "types.h"
#include "l4_types.h"
#include "processor.h"

class Syscall_frame
{
public:
  //protected:
  Mword r[13];
  void dump() const;

  L4_obj_ref ref() const
  { return L4_obj_ref::from_raw(r[2]); }

  void ref(L4_obj_ref const &ref)
  { r[2] = ref.raw(); }

  Utcb *utcb() const
  { return reinterpret_cast<Utcb*>(r[1]); }

  L4_msg_tag tag() const
  { return L4_msg_tag(r[0]); }

  void tag(L4_msg_tag const &tag)
  { r[0] = tag.raw(); }

  L4_timeout_pair timeout() const
  { return L4_timeout_pair(r[3]); }

  void timeout(L4_timeout_pair const &to)
  { r[3] = to.raw(); }

  Mword from_spec() const
  { return r[4]; }

  void from(Mword id)
  { r[4] = id; }
};

class Return_frame
{
public:
  //protected:
  Mword usp;
  Mword ulr;
  Mword km_lr;
  Mword pc;
  Mword psr;

  void psr_set_mode(unsigned char m)
  {
    psr = (psr & ~Proc::Status_mode_mask) | m;
  }

  Mword ip() const
  { return Return_frame::pc; }

  Mword ip_syscall_page_user() const
  { return Return_frame::pc; }

  void ip(Mword _pc)
  { Return_frame::pc = _pc; }

  Mword sp() const
  { return Return_frame::usp; }

  void sp(Mword sp)
  { Return_frame::usp = sp; }

#if defined (CONFIG_CPU_VIRT)
  bool check_valid_user_psr() const
  { return (psr & Proc::Status_mode_mask) != Proc::PSR_m_hyp; }
#else // CONFIG_CPU_VIRT
  bool check_valid_user_psr() const
  { return (psr & Proc::Status_mode_mask) == Proc::PSR_m_usr; }
#endif // CONFIG_CPU_VIRT
};

/**
 * Encapsulation of a syscall entry kernel stack.
 *
 * This class encapsulates the complete top of the 
 * kernel stack after a syscall (including the 
 * iret return frame).
 */
class Entry_frame
: public Syscall_frame,
  public Return_frame
{
public:
  static Entry_frame *to_entry_frame(Syscall_frame *sf)
  { return nonull_static_cast<Entry_frame *>(sf); }

  Syscall_frame *syscall_frame() { return this; }
  Syscall_frame const *syscall_frame() const { return this; }
} __attribute__((packed));


