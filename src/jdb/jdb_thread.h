#pragma once

#include <thread.h>

class Jdb_thread
{
public:
  static void print_state_long(Thread *t, unsigned max_size = 119);
  static void print_snd_partner(Thread *t, int task_format = 0);
  static void print_partner(Thread *t, int task_format = 0);

  static bool has_partner(Thread *t)
  {
    return (t->state() & Thread_ipc_mask) == Thread_receive_wait;
  }

  static bool has_snd_partner(Thread *t)
  {
    return t->state() & Thread_send_wait;
  }


private:
  static unsigned print_state_bits(Mword bits, unsigned max_size);
};

