#pragma once

#include <entry_frame.h>
#include <cstring>
#include <processor.h>

class Thread_arch
{
protected:
  static void init_regs(Entry_frame *r)
  {
    // clear out user regs that can be returned from the thread_ex_regs
    // system call to prevent covert channel
    memset(r, 0, sizeof(*r));
    r->psr = Proc::Status_mode_user;
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



