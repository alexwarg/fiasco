INTERFACE [arm]:

class Trap_state;

EXTENSION class Thread
{
public:
  static void init_per_cpu(Cpu_number cpu, bool resume);
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include <cassert>
#include <cstdio>

#include "globals.h"
#include "kmem.h"
#include "mem_op.h"
#include "static_assert.h"
#include "thread_state.h"
#include "types.h"
#include "warn.h"

#include <thread_vcpu.h>
#include <entry.h>

IMPLEMENT_DEFAULT inline
void
Thread::init_per_cpu(Cpu_number, bool)
{}


//
// Public services
//

// ------------------------------------------------------------------------
IMPLEMENTATION [mp]:

#include "ipi.h"
#include "irq_mgr.h"
#include <sched.h>

EXTENSION class Thread
{
public:
  static void kern_kdebug_ipi_entry() asm("kern_kdebug_ipi_entry");
};

PUBLIC static inline NEEDS["ipi.h"]
void
Thread::handle_debug_remote_requests_irq()
{
  Ipi::eoi(Ipi::Debug, current_cpu());
  Thread::kern_kdebug_ipi_entry();
}

PUBLIC static inline
void
Thread::handle_timer_remote_requests_irq(Upstream_irq const *ui)
{
  Upstream_irq::ack(ui);
  current_thread()->handle_timer_interrupt();
}

class Thread_remote_rq_irq : public Irq_base
{
public:
  // we assume IPIs to be top level, no upstream IRQ chips
  void handle(Upstream_irq const *)
  { Sched<>::handle_remote_requests_irq(); }

  Thread_remote_rq_irq()
  {
    set_hit(&handler_wrapper<Thread_remote_rq_irq>);
    unmask();
  }

  void switch_mode(bool) override {}
};

class Thread_glbl_remote_rq_irq : public Irq_base
{
public:
  // we assume IPIs to be top level, no upstream IRQ chips
  void handle(Upstream_irq const *)
  { Thread::handle_global_remote_requests_irq(); }

  Thread_glbl_remote_rq_irq()
  {
    set_hit(&handler_wrapper<Thread_glbl_remote_rq_irq>);
    unmask();
  }

  void switch_mode(bool) override {}
};

class Thread_debug_ipi : public Irq_base
{
public:
  // we assume IPIs to be top level, no upstream IRQ chips
  void handle(Upstream_irq const *)
  { Thread::handle_debug_remote_requests_irq(); }

  Thread_debug_ipi()
  {
    set_hit(&handler_wrapper<Thread_debug_ipi>);
    unmask();
  }

  void switch_mode(bool) override {}
};

class Thread_timer_tick_ipi : public Irq_base
{
public:
  void handle(Upstream_irq const *ui)
  { Thread::handle_timer_remote_requests_irq(ui); }

  Thread_timer_tick_ipi()
  { set_hit(&handler_wrapper<Thread_timer_tick_ipi>); }

  void switch_mode(bool) override {}
};


//-----------------------------------------------------------------------------
IMPLEMENTATION [mp && !arm_single_ipi_irq && !irregular_gic]:

class Arm_ipis
{
public:
  Arm_ipis()
  {
    check(Irq_mgr::mgr->alloc(&remote_rq_ipi, Ipi::Request, false));
    check(Irq_mgr::mgr->alloc(&glbl_remote_rq_ipi, Ipi::Global_request, false));
    check(Irq_mgr::mgr->alloc(&debug_ipi, Ipi::Debug, false));
    check(Irq_mgr::mgr->alloc(&timer_ipi, Ipi::Timer, false));
  }

  Thread_remote_rq_irq remote_rq_ipi;
  Thread_glbl_remote_rq_irq glbl_remote_rq_ipi;
  Thread_debug_ipi debug_ipi;
  Thread_timer_tick_ipi timer_ipi;
};

static Arm_ipis _arm_ipis;

