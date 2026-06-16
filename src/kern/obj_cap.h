
#pragma once

#include "kobject.h"
#include "thread.h"

class Obj_cap : public L4_obj_ref
{
public:
  Obj_cap(L4_obj_ref const &o) : L4_obj_ref(o) {}

  Kobject_iface *deref(L4_fpage::Rights *rights = 0, bool dbg = false)
  {
    Thread *current = current_thread();
    if (op() & L4_obj_ref::Ipc_reply)
      {
        if (rights) *rights = current->caller_rights();
        Thread *ca = static_cast<Thread*>(current->caller());
        if (EXPECT_TRUE(!dbg && ca))
          current->reset_caller();
        return ca;
      }

    if (EXPECT_FALSE(special()))
      {
        if (!self())
          return 0;

        if (rights) *rights = L4_fpage::Rights::CRWS();
        return current;
      }

    return current->space()->lookup_local(cap(), L4_fpage::Rights::NONE())
      .deref_nocheck(nullptr, rights);
  }
};

