#pragma once

#include <mem_space.h>

template<typename D, typename BASE>
class Kernel_thread_arch : public BASE
{
public:
  using BASE::BASE;
  Address utcb_addr() const
  { return Mem_space::user_max() + 1U - 0x10000U; }
};

