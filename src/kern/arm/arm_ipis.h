#pragma once

#include <thread.h>
#include <sched.h>
#include <ipi.h>
#include <irq_mgr.h>
#include <globalconfig.h>

#include <cassert>

namespace Arm_ipis {

class Remote_request : public Irq_base
{
public:
  // we assume IPIs to be top level, no upstream IRQ chips
  void handle(Upstream_irq const *)
  { Sched<>::handle_remote_requests_irq(); }

  Remote_request()
  {
    set_hit(&handler_wrapper<Remote_request>);
    unmask();
  }

  void switch_mode(bool) override {}
};

class Global_request : public Irq_base
{
public:
  // we assume IPIs to be top level, no upstream IRQ chips
  void handle(Upstream_irq const *)
  { Thread::handle_global_remote_requests_irq(); }

  Global_request()
  {
    set_hit(&handler_wrapper<Global_request>);
    unmask();
  }

  void switch_mode(bool) override {}
};

class Debug : public Irq_base
{
public:
  static void handler()
  {
    Ipi::eoi(Ipi::Debug, current_cpu());
    kern_kdebug_ipi_entry();
  }

  // we assume IPIs to be top level, no upstream IRQ chips
  [[gnu::flatten]]
  void handle(Upstream_irq const *)
  {
    handler();
  }

  Debug()
  {
    set_hit(&handler_wrapper<Debug>);
    unmask();
  }

  void switch_mode(bool) override {}

private:
  static void kern_kdebug_ipi_entry() asm("kern_kdebug_ipi_entry");
};

class Timer : public Irq_base
{
public:
  static void handler(Upstream_irq const *ui = nullptr)
  {
    Upstream_irq::ack(ui);
    current_thread()->handle_timer_interrupt();
  }

  [[gnu::flatten]]
  void handle(Upstream_irq const *ui)
  {
    handler(ui);
  }

  Timer()
  { set_hit(&handler_wrapper<Timer>); }

  void switch_mode(bool) override {}
};


class Base
{
public:
  Remote_request remote_rq_ipi;
  Global_request glbl_remote_rq_ipi;
  Debug debug_ipi;
  Timer timer_ipi;
};

class Ipis : public Base
{
public:
  Ipis()
  {
    check(Irq_mgr::mgr->alloc(&remote_rq_ipi, Ipi::Request, false));
    check(Irq_mgr::mgr->alloc(&glbl_remote_rq_ipi, Ipi::Global_request, false));
    check(Irq_mgr::mgr->alloc(&debug_ipi, Ipi::Debug, false));
    check(Irq_mgr::mgr->alloc(&timer_ipi, Ipi::Timer, false));
  }
};

#ifdef CONFIG_MP
void init_per_cpu(Cpu_number cpu, bool resume);
#else
inline void init_per_cpu(Cpu_number, bool) {}
#endif

} // namespace Arm_ipis
