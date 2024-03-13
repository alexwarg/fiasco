
#include <kobject_helper.h>
#include <pfc.h>
#include <irq_controller.h>

namespace {

class Pfc_obj : public Kobject_h<Pfc_obj, Icu>
{
  enum class Op
  {
    Suspend_system     = 0x0,
    Shutdown_system    = 0x1,
    Allow_cpu_shutdown = 0x2,
    Cpu_hotplug        = 0x3,
  };

  L4_msg_tag sys_cpu_allow_shutdown(L4_fpage::Rights, Syscall_frame *f,
                                    Utcb const *utcb)
  {
    if (f->tag().words() < 3)
      return commit_result(-L4_err::EInval);

    Cpu_number cpu = Cpu_number(access_once(utcb->values + 1));

    if (cpu >= Cpu::invalid() || !Per_cpu_data::valid(cpu))
      return commit_result(-L4_err::EInval);

    return commit_result(Pfc::get()->cpu_allow_shutdown(cpu, access_once(utcb->values + 2)));
  }

  L4_msg_tag
  sys_system_suspend(L4_fpage::Rights, Syscall_frame *f, Utcb const *msg)
  {
    Mword extra = 0;
    if (f->tag().words() >= 2)
      extra = msg->values[1];

    return commit_result(Pfc::get()->system_suspend(extra));
  }

  L4_msg_tag
  sys_system_shutdown(L4_fpage::Rights, Syscall_frame *f, Utcb const *msg)
  {
    if (f->tag().words() != 2)
      return commit_result(-L4_err::EInval);

    if (msg->values[1] == 1) // reboot?
      Pfc::get()->system_reboot();

    if (msg->values[1] == 0) // shutdown
      Pfc::get()->system_off();

    // There's no generic in-kernel way for system power-off
    return commit_result(-L4_err::ENosys);
  }

  L4_msg_tag
  sys_cpu_hotplug(L4_fpage::Rights, Syscall_frame *f, Utcb const *msg)
  {
    if (f->tag().words() < 2)
      return commit_result(-L4_err::EInval);

    Mword phys_id = msg->values[1];
    return commit_result(Pfc::get()->hotplug_cpu(Cpu_phys_id(phys_id)));
  }

  static Pfc_obj pfc;

public:
  L4_msg_tag kinvoke(L4_obj_ref ref, L4_fpage::Rights rights, Syscall_frame *f,
                     Utcb const *r_msg, Utcb *s_msg)
  {
    L4_msg_tag tag = f->tag();

    if (tag.proto() == L4_msg_tag::Label_irq)
      return Icu::icu_invoke(ref, rights, f, r_msg, s_msg);

    if (!Ko::check_basics(&tag, 0))
      return tag;

    switch (static_cast<Op>(r_msg->values[0]))
      {
      case Op::Suspend_system:     return sys_system_suspend(rights, f, r_msg);
      case Op::Shutdown_system:    return sys_system_shutdown(rights, f, r_msg);
      case Op::Allow_cpu_shutdown: return sys_cpu_allow_shutdown(rights, f, r_msg);
      case Op::Cpu_hotplug:        return sys_cpu_hotplug(rights, f, r_msg);
      default:                     return commit_result(-L4_err::ENosys);
      }
  }

};

JDB_DEFINE_TYPENAME(Pfc_obj, "Icu/Pfc");

Pfc_obj Pfc_obj::pfc;

}
