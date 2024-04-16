#pragma once

#include <entry_frame.h>
#include <processor.h>
#include <gdt.h>
#include <thread_arch_bits.h>

class Thread_arch
{
protected:
  static void init_regs(Entry_frame *r)
  {
    // clear out user regs that can be returned from the thread_ex_regs
    // system call to prevent covert channel
    r->flags(EFLAGS_IOPL_K | EFLAGS_IF | 2);	// ei
    r->cs(Gdt::gdt_code_user | Gdt::Selector_user);
    r->ss(Gdt::gdt_data_user | Gdt::Selector_user);

    r->sp(0);
    // after cs initialisation as ip() requires proper cs
    r->ip(0);
  }

  static void
  save_fpu_state_to_utcb(Trap_state *, Utcb *)
  {}

  void vcpu_resume_user_arch()
  {}
};

template<typename THREAD>
class Thread_arch_x : public Thread_arch, public Thread_arch_bits_x<THREAD>
{
private:
  friend class Thread_arch_bits_x<THREAD>;
  using Thread = THREAD;

  Thread *_this() { return static_cast<Thread *>(this); }
  Thread const *_this() const { return static_cast<Thread const *>(this); }

  Context_cpu_state &_cpu_state_() { return _this()->_cpu_state; }

  L4_msg_tag
  sys_gdt_x86(L4_msg_tag tag, Utcb const *utcb, Utcb *out)
  {
    // if no words given then return the first gdt entry
    if (EXPECT_FALSE(tag.words() == 1))
      {
        out->values[0] = Gdt::gdt_user_entry1 >> 3;
        return Kobject_iface::commit_result(0, 1);
      }

    unsigned idx = 0;

    auto &gdt_user_entries = _this()->_cpu_state.gdt_user_entries;

    for (unsigned entry_number = utcb->values[1];
        entry_number < gdt_user_entries.Num
        && Utcb::val_idx(Utcb::val64_idx(2) + idx) < tag.words();
        ++idx, ++entry_number)
      {
        Gdt_entry d = access_once((Gdt_entry *)&utcb->val64[Utcb::val64_idx(2) + idx]);
        if (d.unsafe())
          return Kobject_iface::commit_result(-L4_err::EInval);

        gdt_user_entries[entry_number] = d;
      }

    if (_this() == current())
      gdt_user_entries.load();

    return Kobject_iface::commit_result((utcb->values[1] << 3) + Gdt::gdt_user_entry1 + 3);
  }

  static Return_frame *
  trap_state_to_rf(Trap_state *ts)
  {
    char *im = reinterpret_cast<char*>(ts + 1);
    return reinterpret_cast<Return_frame*>(im)-1;
  }

protected:
  static Mword
  sanitize_user_flags(Mword flags)
  { return (flags & ~(EFLAGS_IOPL | EFLAGS_NT)) | EFLAGS_IF; }

  int sys_control_arch(Utcb const *, Utcb *)
  {
    return 0;
  }

public:
  void restore_exc_state()
  {
    _this()->cont()->restore(_this()->regs());
  }
};

