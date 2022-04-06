
#pragma once

#include "globalconfig.h"
#include "keycodes.h"

class Irq_base;

namespace Vkey
{
enum Echo_type { Echo_off = 0, Echo_on = 1, Echo_crnl = 2 };
void irq(Irq_base *const *i);
void set_echo(Echo_type echo);
void add_char(int v);
int check_();
int get();
void clear();

#if defined (CONFIG_JDB)

inline bool is_debugger_entry_key(int key)
{
  return key == KEY_SINGLE_ESC;
}

#else

inline bool is_debugger_entry_key(int)
{
  return false;
}

#endif

}
