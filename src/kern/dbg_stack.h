#pragma once

#include <globalconfig.h>
#ifdef CONFIG_JDB

#include <config.h>
#include <per_cpu_data.h>

namespace Dbg
{

class Dbg_stack
{
public:
  enum { Stack_size = Config::PAGE_SIZE };
  void *stack_top;
  Dbg_stack();
};

extern Per_cpu<Dbg_stack> dbg_stack;

} // namespace Ddb

#endif // CONFIG_JDB
