#pragma once

#include <entry_frame.h>
#include <trap_state.h>
#include <cstring>
#include <processor.h>
#include <globalconfig.h>
#include <thread_arch_bits.h>

#ifdef CONFIG_FPU
#include <fpu.h>
#endif

class Thread_arch
{
protected:
  static void init_regs(Entry_frame *r)
  {
    // clear out user regs that can be returned from the thread_ex_regs
    // system call to prevent covert channel
    memset(r, 0, sizeof(*r));
    r->psr = Proc::Status_mode_user;
  }

};

template<typename THREAD>
class Thread_arch_x :
  public Thread_arch,
  public Thread_arch_bits_x<THREAD>
{
private:
  friend class Thread_arch_bits_x<THREAD>;

  using Thread = THREAD;

  Thread *_this() { return static_cast<Thread *>(this); }
  Thread const *_this() const { return static_cast<Thread const *>(this); }

public:
#ifdef CONFIG_ARM_LPAE
  // this is here because it is used from JDB code
  static Mword
  is_debug_exception(Mword error_code, bool just_native_type = false)
  {
    if (just_native_type)
      return ((error_code >> 26) & 0x3f) == 0x22;
    return (error_code & 0xc000003f) == 0x80000022;
  }
#else // CONFIG_ARM_LPAE
  // this is here because it is used from JDB code
  static Mword
  is_debug_exception(Mword error_code, bool just_native_type = false)
  {
    if (just_native_type)
      return (error_code & 0x4f) == 2;

    // LPAE type as already converted
    return (error_code & 0xc000003f) == 0x80000022;
  }
#endif // CONFIG_ARM_LPAE

protected:
  void save_fpu_state_to_utcb(Trap_state *ts, Utcb *u)
  {
    (void)ts; (void)u;
#ifdef CONFIG_FPU
    char *esu = (char *)&u->values[21];
    Fpu::save_user_exception_state(_this()->state.has(Thread_fpu_owner),
        _this()->fpu_state(), ts, (Fpu::Exception_state_user *)esu);
#endif
  }

  int sys_control_arch(Utcb const *, Utcb *)
  {
    return 0;
  }

  L4_msg_tag invoke_arch(L4_msg_tag tag, Utcb const *utcb, Utcb *)
  {
    switch (utcb->values[0] & Thread::Opcode_mask)
      {
      case Thread::Op_set_tpidruro_arm:
        return _this()->set_tpidruro(tag, utcb);
      default:
        return Kobject_iface::commit_result(-L4_err::ENosys);
      }
  }

#ifdef CONFIG_ARM_V6PLUS
protected:
  L4_msg_tag set_tpidruro(L4_msg_tag tag, Utcb const *utcb)
  {
    if (EXPECT_FALSE(tag.words() < 2))
      return Kobject_iface::commit_result(-L4_err::EInval);

    _this()->_cpu_state.tpidruro(utcb->values[1]);
    if (EXPECT_FALSE(_this()->state.has(Thread_vcpu_enabled)))
      _this()->arch_update_vcpu_state(_this()->vcpu_state().access());

    if (_this() == current())
      _this()->_cpu_state.load_tpidruro();

    return Kobject_iface::commit_result(0);
  }

public:
  void set_tpidruro(Trex const *t)
  {
    _this()->_cpu_state.tpidruro(access_once(&t->tpidruro));
    if (_this() == current())
      _this()->_cpu_state.load_tpidruro();
  }

  void store_tpidruro(Trex *t)
  {
    t->tpidruro = _this()->_cpu_state.tpidruro();
  }

#else // CONFIG_ARM_V6PLUS
protected:
  L4_msg_tag set_tpidruro(L4_msg_tag, Utcb const *)
  {
    return Kobject_iface::commit_result(-L4_err::EInval);
  }

public:
  void set_tpidruro(Trex const *)
  {}

  void store_tpidruro(Trex *)
  {}

#endif // CONFIG_ARM_V6PLUS
};



