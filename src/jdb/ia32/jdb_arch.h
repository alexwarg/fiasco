#pragma once

#include <globalconfig.h>
#include <per_cpu_data.h>
#include <jdb_entry_frame.h>
#include <string_buffer.h>
#include <types.h>
#include <jdb_types.h>

class Thread;

class Jdb_ia32_base
{
public:
  enum
  {
    Msr_test_default     = 0,
    Msr_test_fail_warn   = 1,
    Msr_test_fail_ignore = 2,
  };

  static Per_cpu<unsigned> apic_tpr;
  static Unsigned16 pic_status;
  static volatile char msr_test;
  static volatile char msr_fail;

  enum Guessed_thread_state
  {
    s_unknown, s_ipc, s_pagefault, s_fputrap,
    s_interrupt, s_timer_interrupt, s_slowtrap, s_user_invoke,
  };

  static int (*bp_test_log_only)(Cpu_number cpu, Jdb_entry_frame *ef);
  static int (*bp_test_sstep)(Cpu_number cpu, Jdb_entry_frame *ef);
  static int (*bp_test_break)(Cpu_number cpu, Jdb_entry_frame *ef, String_buffer *buf);
  static int (*bp_test_other)(Cpu_number cpu, Jdb_entry_frame *ef, String_buffer *buf);

  static bool handle_special_cmds(int c);
  // The content of apdapter memory is not shown by default because reading
  // memory-mapped I/O registers may confuse the hardware. We assume that all
  // memory above the end of the RAM is adapter memory.
  static int is_adapter_memory(Jdb_address addr);
  static bool test_checksums();

  static Guessed_thread_state guess_thread_state(Thread *t);

  static bool connected()
  {
    return _connected;
  }

  static void peek_phys(Address phys, void *value, int width);
  static void poke_phys(Address phys, void const *value, int width);
  static Address get_phys_address(Jdb_address addr);
  static void set_single_step(Cpu_number cpu, int on);

  static char _connected;
  static Per_cpu<char> permanent_single_step;
  static Per_cpu<char> code_ret, code_call, code_bra, code_int;

  enum Step_state
  {
    SS_NONE=0, SS_BRANCH, SS_RETURN
  };

  static Per_cpu<Step_state> ss_state;
  static Per_cpu<int> ss_level;

  static const Unsigned8 *debug_ctrl_str;
  static int              debug_ctrl_len;

  static Per_cpu<int> jdb_irqs_disabled;
  static int get_register(char *reg);

#ifdef CONFIG_SERIAL
    ;
#else
  {}
#endif
};

using Jdb_arch = Jdb_ia32_base;

