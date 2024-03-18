#pragma once

#include "kernel_thread.h"

class App_cpu_thread : public Kernel_thread
{
public:
  static Kernel_thread *may_be_create(Cpu_number cpu, bool cpu_never_seen_before);
  explicit App_cpu_thread(Ram_quota *q) : Kernel_thread(q) {}

private:
  void bootstrap(Mword resume) asm ("call_ap_bootstrap") FIASCO_FASTCALL;
};

