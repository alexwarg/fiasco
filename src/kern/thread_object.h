#pragma once

#include "kobject.h"
#include "kobject_helper.h"
#include "thread.h"
#include "obj_cap.h"

class Thread_object : public Thread, public Kobject_helper_base
{
public:
  explicit Thread_object(Ram_quota *q)
  : Thread(q) {}

  explicit Thread_object(Ram_quota *q, Context_mode_kernel k)
  : Thread(q, k) {}

  void operator delete(void *_t) noexcept;

  bool put() override
  { return dec_ref() == 0; }

  void destroy(Kobject ***rl) override;

  void invoke(L4_obj_ref self, L4_fpage::Rights rights,
              Syscall_frame *f, Utcb *utcb) override;

  bool ex_regs(Address ip, Address sp,
               Address *o_ip = nullptr, Address *o_sp = nullptr,
               Mword *o_flags = nullptr, Mword ops = 0);

private:
  struct Remote_syscall
  {
    Thread *thread;
    L4_msg_tag result;
    bool have_recv;
  };

  L4_msg_tag sys_vcpu_resume(L4_msg_tag const &tag, Utcb const *utcb, Utcb *);
  L4_msg_tag sys_modify_senders(L4_msg_tag tag, Utcb const *in, Utcb *out);
  L4_msg_tag sys_register_delete_irq(L4_msg_tag tag, Utcb const *in, Utcb *out);
  L4_msg_tag sys_control(L4_fpage::Rights rights, L4_msg_tag tag,
                         Utcb const *utcb, Utcb *out);
  L4_msg_tag sys_vcpu_control(L4_fpage::Rights, L4_msg_tag const &tag,
                              Utcb const *utcb, Utcb *out);
  L4_msg_tag sys_ex_regs(L4_msg_tag const &tag, Utcb *utcb, Utcb *out);
  L4_msg_tag sys_thread_stats(L4_msg_tag const &tag, Utcb const *utcb, Utcb *out);
  L4_msg_tag ex_regs(Utcb const *utcb, Utcb *out);
  L4_msg_tag sys_thread_switch(L4_msg_tag const &tag, Utcb const *utcb,Utcb *out);


  static Drq::Result handle_remote_ex_regs(Drq *, Context *self, void *p);
  Drq::Result sys_thread_stats_remote(void *data);
  static Drq::Result handle_sys_thread_stats_remote(Drq *, Context *self, void *data);
};


