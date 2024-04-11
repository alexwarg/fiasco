
#include <context_drq.h>
#include <context.h>

DEFINE_PER_CPU Per_cpu<Context_drq_base::Kernel_drq> Context_drq_base::_kernel_drq;

bool
Context::handle_drq()
{

  assert (check_for_current_cpu());
  assert (cpu_lock.test());

  bool resched = false;
  Mword st = state();
  if (EXPECT_FALSE(st & Thread_switch_hazards))
    {
      state.del_dirty(Thread_switch_hazards);
      if (st & Thread_finish_migration)
        finish_migration();

      if (st & Thread_need_resched)
        resched = true;
    }

  if (EXPECT_TRUE(!drq_pending()))
    return resched;

  Mem::barrier();
  resched |= _drq_q.handle_requests(this);
  state.del_dirty(Thread_drq_ready);

  //LOG_MSG_3VAL(this, "xdrq", state(), 0, cpu_lock.test());

  return resched || !(state.has(Thread_ready_mask));
}

