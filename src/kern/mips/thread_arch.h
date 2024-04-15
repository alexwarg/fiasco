#pragma once

#include <entry_frame.h>
#include <cstring>
#include <cp0_status.h>

class Thread_arch
{
protected:
  static void init_regs(Entry_frame *r)
  {
    // clear out user regs that can be returned from the thread_ex_regs
    // system call to prevent covert channel
    memset(r, 0, sizeof(*r));
    r->status = Cp0_status::status_eret_to_user_ei(Cp0_status::read());
  }

};

template<typename THREAD>
class Thread_arch_x : public Thread_arch
{
private:
  using Thread = THREAD;

  Thread *_this() { return static_cast<Thread *>(this); }
  Thread const *_this() const { return static_cast<Thread const *>(this); }

public:
};



