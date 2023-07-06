#pragma once

#include <thread_object.h>
#include <processor.h>
#include <config.h>
#include <cpu_idle_iface.h>
#include <kernel_thread-arch.h>

class Kernel_thread_defaults
{
public:
  void clean_initcall_section()
  {}

  Address utcb_addr() const
  { return Mem_layout::Utcb_addr; }
};

class Kernel_thread :
  public Thread_object,
  public Kernel_thread_arch<Kernel_thread, Kernel_thread_defaults>
{
public:
  explicit Kernel_thread(Ram_quota *q)
  : Thread_object(q, Thread::Kernel)
  {}

  Mword *init_stack()
  {
    return _cpu_state.kernel_sp;
  }

  static Cpu_idle_iface *idle;
private:
  /**
   * Frees the memory of the initcall sections.
   *
   * Virtually initcall sections are freed by not marking them
   * reserved in the KIP. This method just invalidates the contents of
   * the memory, by filling it with some invalid data and may be
   * unmapping it.
   */
  void bootstrap() asm ("call_bootstrap") FIASCO_FASTCALL;
  void bootstrap_arch();
  void run();

protected:
  void init_workload();
};



