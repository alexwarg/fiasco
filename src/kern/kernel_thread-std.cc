#include <kernel_thread.h>

#include <assert_opt.h>
#include <config.h>
#include <factory.h>
#include <initcalls.h>
#include <ipc_gate.h>
#include <irq.h>
#include <kip.h>
#include <koptions.h>
#include <map_util_objs.h>
#include <map_util_mem.h>
#include <map_util_io.h>
#include <mem_layout.h>
#include <sigma0_task.h>
#include <task.h>
#include <thread_object.h>
#include <types.h>
#include <ram_quota.h>
#include <task_factory_impl.h>

static constexpr Cap_index C_task    = Cap_index(Initial_kobjects::Task);
static constexpr Cap_index C_thread  = Cap_index(Initial_kobjects::Thread);
static constexpr Cap_index C_factory = Cap_index(Initial_kobjects::Factory);
static constexpr Cap_index C_pager   = Cap_index(Initial_kobjects::Pager);


inline Task *create_sigma0_task()
{
  int err;
  Task *sigma0 = Task::create<Sigma0_task>(Ram_quota::root, L4_msg_tag(), 0, &err);
  assert_opt (sigma0);
  // prevent deletion of the sigma0 task
  sigma0->inc_ref();

  init_mapdb_mem(sigma0);
  init_mapdb_io(sigma0);

  check (map(sigma0,          sigma0, sigma0, C_task, 0));
  check (map(Factory::root(), sigma0, sigma0, C_factory, 0));

  for (Cap_index c = Initial_kobjects::first(); c < Initial_kobjects::end(); ++c)
    {
      Kobject_iface *o = initial_kobjects.obj(c);
      if (o)
        check(map(o, sigma0, sigma0, c, 0));
    }
  return sigma0;
}

inline Task *create_boot_task(Task *sigma0, Thread *sigma0_thread)
{
  int err;
  Task *boot_task = Task::create<Task>(Ram_quota::root, L4_msg_tag(), 0, &err);
  assert_opt (boot_task);
  // prevent deletion of the boot task
  boot_task->inc_ref();

  check (map(boot_task,   boot_task, boot_task, C_task, 0));
  check (obj_map(sigma0, C_factory, 1, boot_task, C_factory, 0).error() == 0);

  auto *s0_b_gate = Ipc_gate::create(Ram_quota::root, sigma0_thread, 4 << 4);

  check (s0_b_gate);
  check (map(s0_b_gate, boot_task, boot_task, C_pager, 0));

  for (Cap_index c = Initial_kobjects::first(); c < Initial_kobjects::end(); ++c)
    {
      Kobject_iface *o = initial_kobjects.obj(c);
      if (o)
        check(obj_map(sigma0, c, 1, boot_task, c, 0).error() == 0);
    }
  return boot_task;
}

inline Thread_object *create_user_thread(Kernel_thread *kt, Task *task, Thread_ptr const &pager, Address ip)
{
  Thread_object *thread = new (Ram_quota::root) Thread_object(Ram_quota::root);
  assert_opt(thread);
  // prevent deletion of this thing
  thread->inc_ref();

  check (map(thread, task, task, C_thread, 0));

  // Task just newly created, no need for locking or remote TLB flush.
  check(task->alloc_ku_mem(L4_fpage::mem(kt->utcb_addr(), Config::PAGE_SHIFT),
                           false) >= 0);

  check (thread->control(pager, Thread_ptr(Thread_ptr::Null)) == 0);
  check (thread->bind(task, User_ptr<Utcb>(reinterpret_cast<Utcb*>(kt->utcb_addr()))));
  check (thread->ex_regs(ip, 0));

  thread->set_home_cpu(Cpu_number::boot_cpu());
  return thread;
}

void
Kernel_thread::init_workload()
{
  auto g = lock_guard(cpu_lock);

  // create sigma0
  Task *sigma0 = create_sigma0_task();
  Thread_object *sigma0_thread =
    create_user_thread(this, sigma0, Thread_ptr(Thread_ptr::Null), Kip::k()->sigma0_ip);

  sigma0_thread->activate();

  // create the boot task
  Task *boot_task = create_boot_task(sigma0, sigma0_thread);
  Thread_object *boot_thread =
    create_user_thread(this, boot_task, Thread_ptr(C_pager), Kip::k()->root_ip);

  boot_thread->activate();
}
