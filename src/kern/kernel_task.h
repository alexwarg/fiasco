#pragma once

#include "task.h"
#include "types.h"
#include <kernel_task_kdir.h>

class Kernel_thread;

class Kernel_task : public Task
{
  friend class Kernel_thread;
  friend class Static_object<Kernel_task>;
private:
  static Static_object<Kernel_task> _t;

  Kernel_task()
  : Task(Ram_quota::root, Kernel_task_kdir::get(), Caps::none())
  {}

public:
  static Task *kernel_task()
  { return _t; }

  static void init()
  { _t.construct(); }
};

