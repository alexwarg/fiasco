#pragma once

#include <tb_entry.h>
#include <string_buffer.h>

class Context;
class Sched_context;

/** logged context switch. */
class Tb_entry_ctx_sw : public Tb_entry
{
public:
  using Tb_entry::_ip;

  Context const *dst;		///< switcher target
  Context const *dst_orig;
  Address kernel_ip;
  Mword lock_cnt;
  Space const *from_space;
  Sched_context const *from_sched;
  Mword from_prio;
  void print(String_buffer *buf) const;
};

struct Migration_log : public Tb_entry
{
  Mword    state;
  Address  user_ip;
  Cpu_number src_cpu;
  Cpu_number target_cpu;

  void print(String_buffer *) const;
};

