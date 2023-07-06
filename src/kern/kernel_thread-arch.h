#pragma once

// default, empty kernel thread arch CRTP

template<typename D, typename BASE>
class Kernel_thread_arch : public BASE
{
public:
  using BASE::BASE;
};
