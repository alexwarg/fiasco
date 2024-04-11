#pragma once

#include <types.h>
#include <context.h>

template<typename T = void>
class Sched;

template<>
class Sched<void>
{
public:
  static void
  migrate(Context *c, Context::Migration *info);

  static bool
  take_cpu_offline(Cpu_number cpu, bool drain_rqq = false);

  static void
  handle_remote_requests_irq() asm ("handle_remote_cpu_requests");

  static void
  force_to_invalid_cpu(Context *c);
};
