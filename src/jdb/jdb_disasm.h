#pragma once

#include <globalconfig.h>
#include "jdb_module.h"
#include "jdb_types.h"
#include "l4_types.h"
#include "types.h"

class Space;

#ifdef CONFIG_JDB_DISASM

class Jdb_disasm : public Jdb_module
{
public:
  Jdb_disasm() FIASCO_INIT;
  static bool avail() { return true; }

  static bool show_disasm_line(int len, Jdb_address &addr);
  static Jdb_module::Action_code show(Jdb_address virt, int level);

  Action_code action(int cmd, void *&args, char const *&fmt, int &next_char) override;
  Cmd const *cmds() const override;
  int num_cmds() const override;

private:
  static char show_intel_syntax;
  static char show_arm_thumb;

  static bool disasm_line(char *buffer, int buflen, Jdb_address &addr);
  static Address disasm_offset(Jdb_address &start, int offset);
  static bool disasm_offset_decr(Jdb_address &addr);
  static bool disasm_offset_incr(Jdb_address &addr);
};

#else

class Jdb_disasm
{
public:
  static bool avail() { return false; }
  static Jdb_module::Action_code show(Jdb_address, int)
  { return Jdb_module::NOTHING; }
  static bool show_disasm_line(int, Jdb_address &)
  { return false; }
};

#endif
