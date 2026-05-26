

#pragma once

#include <context.h>
#include <types.h>
#include <trap_state.h>

template<typename T>
class Jdb_tcb_ptr_arch
{
public:
  Address user_ip() const
  {
    return static_cast<T const *>(this)->top_value(-1);
  }

  static bool arch_is_user_value(Address _offs)
  {
    return _offs >= Context::Size - sizeof(Trap_state);
  }

  static const char *arch_user_value_desc(Address _offs)
  {
    const char *const desc[] =
      {
        "Epc", "Status", "Cause", "BadVAddr", "Lo", "Hi",
        "ra ($31)", "s8 ($30)", "sp ($29)", "gp ($28)",
        "k1 ($27)", "k0 ($26)", "t9 ($25)", "t8 ($24)",
        "s7 ($23)", "s6 ($22)", "s5 ($21)", "s4 ($s0)",
        "s3 ($19)", "s2 ($18)", "s1 ($17)", "s0 ($16)",
        "t7 ($15)", "t6 ($14)", "t5 ($13)", "t4 ($12)",
        "t3 ($11)", "t2 ($10)", "t1 ($9)",  "t0 ($8)",
        "a3 ($7)",  "a2 ($6)",  "a1 ($5)",  "a0 ($4)",
        "v1 ($3)",  "v0 ($2)",  "at ($1)",  "eret-work",
        "BadInstr", "BadInstrP"
      };
    static_assert ((sizeof (Trap_state) / sizeof (Mword))
                   <= (sizeof (desc) / sizeof (desc[0])),
                   "desc entries do not match the sizeof Trap_state");
    return desc[(Context::Size - _offs) / sizeof(Mword) - 1];
  }
};
