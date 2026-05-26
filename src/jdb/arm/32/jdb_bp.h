#pragma once

#include <jdb.h>
#include <jdb_input.h>
#include <jdb_input_task.h>
#include <jdb_module.h>

class Jdb_bp : public Jdb_module_mixin<Jdb_bp>, public Jdb_input_task_addr
{
public:
  Jdb_bp() FIASCO_INIT;
  Action_code action(int cmd, void *&args, char const *&fmt, int &next_char) override;
  static cxx::static_vector<Cmd const> jdb_cmds()
  {
    static Cmd cs[] =
      {
          {
            0, "b", "bp", "%c",
            "b{i|a|r|w}<addr>\tset breakpoint on insn/access/read/write "
            "access\n"
            "b-{b|w}<nr>\tdisable breakpoint\n"
            "bl\tlist breakpoints\n"
            "bI\tshow info on hw debugging",
            &breakpoint_cmd
          },
      };

    return cs;
  }


private:
  static void   at_jdb_enter();
  static void   at_jdb_leave();

  static int    test_log_only(Cpu_number);
  static int    test_break(Cpu_number, String_buffer *buf);


  static char  breakpoint_cmd;
  static bool inited;
  static char state;
  static char breakpoint_length;
  static int  breakpoint_number;
  static char breakpoint_type;

  enum
  {
    Vers_v7_1 = 5,
  };

  static void init_cpu(Cpu_number);
  static unsigned num_watchpoints();
  static unsigned num_breakpoints();
  static void wvr(int num, Mword v);
  static Mword wvr(int num);
  static void wcr(int num, Mword v);
  static Mword wcr(int num);
  static void bvr(int num, Mword v);
  static Mword bvr(int num);
  static void bcr(int num, Mword v);
  static Mword bcr(int num);
  static void test_debug(Cpu_number cpu, String_buffer *buf, char *type,
                         bool *disable, Address *addr);
  static void disable_breakpoint(Address addr);
  static Mword instruction_bp_at_addr(Address addr);
  static void disable_watchpoint(Address addr);
  static void set_bw(int idx, char type, Address addr,
                     Mword cr_mask, Mword cr_val);

  void wp_bas(String_buffer *b, unsigned bas);
  void show_wp(unsigned idx);
  void show_bp(unsigned idx);
  void show_bps();

  static const char *version_string(unsigned vers);
  static Mword dbgdidr()
  {
    Mword v;
    asm volatile("mrc p14, 0, %0, c0, c0, 0" : "=r" (v));
    return v;
  }

  static Mword hw_version()
  {
    return (dbgdidr() >> 16) & 0xf;
  }

  static void show_hwinfo_cpu(Cpu_number cpu);
  static bool dbg_avail();

  int get_free_wp();
  int get_free_bp();
};

