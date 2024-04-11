#pragma once

class Thread;

class Thread_no_fpu
{
public:

  int switchin_fpu(bool alloc_new_fpu = true)
  {
    (void)alloc_new_fpu;
    return 0;
  }

  bool alloc_eager_fpu_state()
  {
    return true;
  }

  void transfer_fpu(Thread *)
  {}

protected:
  void free_fpu_state()
  {}
};


template<typename TH>
using Thread_fpu_x = Thread_no_fpu;

