#pragma once

#include <string_buffer.h>
#include <jdb_types.h>
#include <initcalls.h>
#include <l4_types.h>
#include <jdb_entry_frame.h>
#include <jdb_bp-ia32-bits.h>


class Thread;
class Task;
class Space;

class Breakpoint
{
public:
  enum Mode { INSTRUCTION=0, WRITE=1, PORTIO=2, ACCESS=3 };
  enum Log  { BREAK=0, LOG=1 };

  Breakpoint()
  {
    restrict.thread.thread = 0;
    restrict.task.task     = 0;
  }

  void kill()
  {
    addr = Jdb_address::null();
  }

  int unused()
  {
    return addr == Jdb_address::null();
  }

  int break_at_instruction()
  {
    return mode == INSTRUCTION;
  }

  int match_addr(Jdb_address virt, Mode m)
  {
    return !unused() && addr == virt && mode == m;
  }

  void set_logmode(char m)
  {
    log = (m == '*') ? LOG : BREAK;
  }

  int is_break()
  {
    return !unused() && log == BREAK;
  }

  void restrict_task(int other, Mword task)
  {
    restrict.task.other = other;
    restrict.task.task  = task;
  }

  void restrict_thread(int other, Mword thread)
  {
    restrict.thread.other  = other;
    restrict.thread.thread = thread;
  }

  void restrict_register(char reg, Mword y, Mword z)
  {
    restrict.reg.reg = reg;
    restrict.reg.y   = y;
    restrict.reg.z   = z;
  }

  void restrict_memory(Mword addr, Mword len, Mword y, Mword z)
  {
    restrict.mem.addr = addr;
    restrict.mem.len  = len;
    restrict.mem.y    = y;
    restrict.mem.z    = z;
  }

  void clear_restriction()
  {
    restrict.thread.thread = 0;
    restrict.task.task     = 0;
    restrict.reg.reg       = 0;
    restrict.mem.len       = 0;
  }

  void set(Jdb_address _addr, Mword _len, Mode _mode, Log _log)
  {
    addr = _addr;
    mode = _mode;
    log  = _log;
    len  = _len;
  }

  int restricted(Thread *t);
  int test_break(String_buffer *buf, Thread *t);
  void test_log(Thread *t);
  void show();

private:
  typedef struct
    {
      int other;
      Mword thread;
    } Bp_thread_res;

  typedef struct
    {
      int other;
      Mword task;
    } Bp_task_res;

  typedef struct
    {
      char reg;
      Address y, z;
    } Bp_reg_res;

  typedef struct
    {
      unsigned char len;
      Address addr;
      Address y, z;
    } Bp_mem_res;

  typedef struct
    {
      Bp_thread_res thread;
      Bp_task_res   task;
      Bp_reg_res    reg;
      Bp_mem_res    mem;
    } Restriction;

  Jdb_address  addr;
  Unsigned8    len;
  Mode         mode;
  Log          log;
  Restriction  restrict;
  static char const * const mode_names[4];
};


class Jdb_bp
{
public:
  static void init_arch();

  static void clr_dr7(int num, Mword &dr7)
  {
    dr7 &= ~(((3 + (3 <<2)) << (16 + 4 * num)) + (3 << (2 * num)));
  }

  static void set_dr7(int num, Mword len, Breakpoint::Mode mode, Mword &dr7)
  {
    // the encoding of length 8 is special
    if (len == 8)
      len = 3;

    dr7 |= ((((mode & 3) + ((len-1)<<2)) << (16 + 4 * num)) + (2 << (2 * num)));
    dr7 |= 0x200; /* exact breakpoint enable (not available on P6 and below) */
  }

  static int set_debug_address_register(int num, Jdb_address addr, Mword len,
                                        Breakpoint::Mode mode);

  static ALWAYS_INLINE
  void write_debug_register(unsigned num, Mword val)
  {
    asm volatile("mov %0, %%db%c1" :: "r" (val), "i"(num));
  }

  static ALWAYS_INLINE
  Mword read_debug_register(unsigned num)
  {
    Mword val;
    asm volatile("mov %%db%c1, %0" : "=r"(val) : "i"(num));
    return val;
  }

  static ALWAYS_INLINE
  Mword get_dr(Mword i)
  {
    switch (i)
      {
      case 0: return read_debug_register(0);
      case 1: return read_debug_register(1);
      case 2: return read_debug_register(2);
      case 3: return read_debug_register(3);
      case 6: return read_debug_register(6);
      case 7: return dr7;
      default: return 0;
      }
  }

  static int test_break(Cpu_number cpu, Jdb_entry_frame *e, String_buffer *buf, Mword dr6);
  // Create log entry if breakpoint matches.
  // Return 1 if debugger should stop
  static void test_log(Mword &dr6);
  static Mword test_match(Jdb_address addr, Breakpoint::Mode mode);

  static int instruction_bp_at_addr(Jdb_address addr)
  { return test_match(addr, Breakpoint::INSTRUCTION); }

  static void restrict_task(int num, int other, Mword task)
  {
    bps[num].restrict_task(other, task);
  }

  static void restrict_thread(int num, int other, Mword thread)
  {
    bps[num].restrict_thread(other, thread);
  }

  static void restrict_register(int num, char reg, Mword y, Mword z)
  {
    bps[num].restrict_register(reg, y, z);
  }

  static void restrict_memory(int num, Mword addr, Mword len, Mword y, Mword z)
  {
    bps[num].restrict_memory(addr, len, y, z);
  }

  static void clear_restriction(int num)
  {
    bps[num].clear_restriction();
  }

  static void clr_breakpoint(int num)
  {
    clr_debug_address_register(num);
    bps[num].kill();
  }

  static void logmode_breakpoint(int num, char mode)
  {
    bps[num].set_logmode(mode);
  }

  static void list();
  static int set_breakpoint(int num, Jdb_address addr, Mword len,
                            Breakpoint::Mode mode, Breakpoint::Log log);
  static int first_unused()
  {
    int i;

    for (i = 0; i < 4 && !bps[i].unused(); i++)
      ;

    return i;
  }


private:
  static int test_sstep(Cpu_number cpu, Jdb_entry_frame *ef);
  static int test_break(Cpu_number cpu, Jdb_entry_frame *ef, String_buffer *buf);
  static int test_other(Cpu_number cpu, Jdb_entry_frame *ef, String_buffer *buf);
  static int test_log_only(Cpu_number cpu, Jdb_entry_frame *ef);
  static Mword dr7;

  static void at_jdb_enter();
  static void at_jdb_leave();
  static Breakpoint bps[4];

  static void clr_debug_address_register(int num)
  {
    clr_dr7(num, dr7);
  }

};
