#pragma once

#include <entry_frame.h>
#include <processor.h>
#include <gdt.h>

class Thread_arch
{
protected:
  static void init_regs(Entry_frame *r)
  {
    // clear out user regs that can be returned from the thread_ex_regs
    // system call to prevent covert channel
    r->flags(EFLAGS_IOPL_K | EFLAGS_IF | 2);	// ei
    r->cs(Gdt::gdt_code_user | Gdt::Selector_user);
    r->ss(Gdt::gdt_data_user | Gdt::Selector_user);

    r->sp(0);
    // after cs initialisation as ip() requires proper cs
    r->ip(0);
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



