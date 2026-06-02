
#include <sched.h>
#include <context.h>

#include <globalconfig.h>

#ifdef CONFIG_MP
#include <sched_mp.h>
#else
#include <sched_up.h>
#endif

#include <sched_context.h>

template<>
class Sched<int> : public Sched<Sched<int>>
{
public:
  using Migration = Context::Migration;

  class Migration_start_result
  {
  public:
    enum Special
    {
      Resched = 1,
      Nothing = 2
    };

    Migration_start_result() = default;

    Migration_start_result(Context::Migration *mig) noexcept
    { _d.m = mig; }

    Migration_start_result(Special s) noexcept
    { _d.w = static_cast<Mword>(s); }

    bool is_done() const noexcept
    { return _d.w & 3; }

    // \pre is_done() == true
    bool resched() const noexcept
    { return _d.w & 1; }

    // \pre is_done() == false
    Context::Migration *get() const noexcept
    { return _d.m; }

  private:
    union D
    {
      Mword w;
      Context::Migration *m;
    };

    D _d;
  };

  static void
  set_sched_params(Context *c, L4_sched_param const *p)
  {
    Sched_context *sc = c->sched_context();

    // this can actually access the ready queue of a CPU that is offline remotely
    Sched_context::Ready_queue &rq = Sched_context::rq.cpu(c->home_cpu());
    rq.ready_dequeue(c->sched());

    sc->set(p);
    sc->replenish();

    if (sc == rq.current_sched())
      rq.set_current_sched(sc);

    if (c->state.has(Thread_ready_mask)) // maybe we could ommit enqueueing current
      rq.ready_enqueue(c->sched());
  }

  static Migration_start_result
  start_migration(Context *c)
  {
    assert(cpu_lock.test());
    Context::Migration *m = c->_migration.get_and_clear();
    assert (!(reinterpret_cast<Mword>(m) & 0x3)); // ensure alignment

    if (!m)
      return Migration_start_result::Nothing; // bit one == 0 --> no need to reschedule

    if (m->cpu == c->home_cpu())
      {
        set_sched_params(c, m->sp);
        m->set_done();
        return Migration_start_result::Resched; // bit one == 1 --> need to reschedule
      }

    return m; // need to do real migration
  }

  static bool
  migrate_ready_dequeue(Context *c, bool remote)
  {
    auto *rq = migrate_get_ready_queue(c, remote);
    if (!rq)
      return false;

    // if we are in the middle of the scheduler, leave it now
    if (rq->schedule_in_progress == c)
      rq->schedule_in_progress = nullptr;

    rq->ready_dequeue(c->sched());

    // Not sure if this can ever happen
    Sched_context *csc = rq->current_sched();
    if (remote)
      return false;

    if (csc != c->sched())
      return false;

    rq->set_current_sched(Context::kernel_context(current_cpu())->sched());
    return true;
  }

  [[gnu::nonnull]]
  static void
  migrate_finish_sched(Context *c, Migration *inf)
  {
    Sched_context *sc = c->sched_context();
    sc->set(inf->sp);
    sc->replenish();
    c->set_sched(sc);

    Mem::mp_wmb();

    // The migration must be finished on the new CPU core before executing any
    // userland code. This will be done by Context::switch_handle_drq() after
    // the next context switch to this context was performed on the new CPU.
    c->state.add_dirty(Thread_finish_migration);
    c->set_home_cpu(inf->cpu);
    inf->set_done();
  }

  [[gnu::flatten]]
  static bool
  migrate_away(Context *c, Context::Migration *inf, bool remote)
  {
    assert (current() != c);
    assert (cpu_lock.test());

    if (c->_timeout)
      c->_timeout->reset();

    bool resched = migrate_ready_dequeue(c, remote);
    [[gnu::unused]] auto g = lock_and_dequeue_rqq(c, !remote);
    migrate_finish_sched(c, inf);
    return resched;
  }

  static Context::Drq::Result
  handle_migration_helper(Context::Drq *rq, Context *, void *p)
  {
    Migration *inf = reinterpret_cast<Migration *>(p);
    Context *v = static_cast<Context::Kernel_drq*>(rq)->src;
    do_migration_not_current(v, inf);
    return Drq::no_answer_resched();
  }

  static bool
  do_migration(Context *c)
  {
    auto inf = start_migration(c);

    if (inf.is_done())
      return inf.resched(); // already migrated, nothing to do

    c->spill_fpu_if_owner();

    if (current() == c)
      {
        assert (current_cpu() == c->home_cpu());
        return c->kernel_context_drq(handle_migration_helper, inf.get());
      }
    else
      return do_migration_not_current(c, inf.get()); // we already are chosen by the scheduler...
  }

  [[gnu::flatten]]
  static bool
  initiate_migration(Context *c)
  {
    assert (current() != c);
    auto inf = start_migration(c);

    if (inf.is_done())
      return inf.resched();

    c->spill_fpu_if_owner();
    return do_migration_not_current(c, inf.get());
  }

private:
  [[gnu::flatten]]
  static bool
  do_migration_not_current(Context *v, Migration *m)
  {
    Cpu_number target_cpu = access_once(&m->cpu);
    bool resched = migrate_away(v, m, false);
    resched |= migrate_to(v, target_cpu, false);
    return resched;
  }

};


[[gnu::flatten]]
void
Sched<void>::migrate(Context *c, Context::Migration *info)
{
  Sched<int>::migrate(c, info);
}

[[gnu::flatten]]
void
Sched<void>::force_to_invalid_cpu(Context *c)
{
  Sched<int>::force_to_invalid_cpu(c);
}

#ifdef CONFIG_MP

[[gnu::flatten]]
bool
Sched<void>::take_cpu_offline(Cpu_number cpu, bool drain_rqq)
{
  return Sched<int>::take_cpu_offline(cpu, drain_rqq);
}

[[gnu::flatten]]
void
Sched<void>::handle_remote_requests_irq()
{
  Sched<int>::handle_remote_requests_irq();
}

#endif // CONFIG_MP
