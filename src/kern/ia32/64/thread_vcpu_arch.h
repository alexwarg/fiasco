#pragma once

#include <context.h>
#include <vcpu.h>
#include <thread_vcpu_ia32.h>

template<typename BASE>
class Thread_vcpu_arch_t : public Thread_vcpu_ia32_t<BASE>
{
private:
  using B = Thread_vcpu_ia32_t<BASE>;

public:
  static void
  init_state(Context *c, Vcpu_state *v, bool ext)
  {
    v->host.fs_base = *c->fs_base();
    v->host.gs_base = *c->gs_base();
    v->host.ds = 0;
    v->host.es = 0;
    v->host.fs = 0;
    v->host.gs = 0;
    v->host.user_ds32 = Gdt::gdt_data_user | Gdt::Selector_user;
    v->host.user_cs64 = Gdt::gdt_code_user | Gdt::Selector_user;
    v->host.user_cs32 = Gdt::gdt_code_user32 | Gdt::Selector_user;

    B::init_state(c, v, ext);
  }
};
