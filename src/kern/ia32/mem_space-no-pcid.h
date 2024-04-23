#pragma once

#include <mem_unit.h>

template<typename M>
class Mem_space_ia32_no_pcid
{
private:
  using Mem_space = M;
  Mem_space *_ths() { return static_cast<Mem_space *>(this); }

protected:
  void tlb_flush_this_()
  {
    if (Mem_space::current_mem_space() == _ths())
      Mem_unit::tlb_flush();
  }

  void reset_asid()
  {}


};
