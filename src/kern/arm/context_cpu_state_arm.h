#pragma once

#include <types.h>
#include <globalconfig.h>

struct Context_cpu_state_arm
{
#ifdef CONFIG_ARM_V6PLUS
  Mword _tpidrurw;
  Mword _tpidruro;

  Mword tpidrurw() const
  {
    return _tpidrurw;
  }

  Mword tpidruro() const
  {
    return _tpidruro;
  }

  Mword tpidrurw(Mword v)
  {
    return _tpidrurw = v;
  }

  Mword tpidruro(Mword v)
  {
    return _tpidruro = v;
  }

#else
  void store_tpidrurw() const
  {}

  void load_tpidrurw() const
  {}

  void load_tpidruro() const
  {}

  Mword tpidrurw() const
  {
    return 0;
  }

  Mword tpidruro() const
  {
    return 0;
  }

  Mword tpidrurw(Mword)
  {
    return 0;
  }

  Mword tpidruro(Mword)
  {
    return 0;
  }
#endif
};
