#pragma once

#include <mmio_register_block.h>
#include <arm_mptimer.h>

struct Scu
{
  enum R
  {
    Control      = 0x0,
    Config       = 0x4,
    Power_status = 0x8,
    Inv          = 0xc,
    SAC          = 0x50,
    SNSAC        = 0x54,
  };

  enum
  {
    Bsp_enable_bits = 0,
    Available = 1,
  };

  enum Control
  {
    Ic_standby     = 1 << 6,
    Scu_standby    = 1 << 5,
    Force_port0    = 1 << 4,
    Spec_linefill  = 1 << 3,
    Ram_parity     = 1 << 2,
    Addr_filtering = 1 << 1,
    Enable         = 1 << 0,
  };

  Register_block<32> r;

  void reset() const
  {
    r[R::Inv] = 0xffffffff;
  }

  void enable(Mword bits = 0) const
  {
    Unsigned32 ctrl = r[R::Control];
    if (!(ctrl & Control::Enable))
      r[R::Control] = ctrl | bits | Control::Enable;
  }

  Mword config() const
  {
    return r[R::Config];
  }

  template<typename ...T>
  Scu(T &&...args) : r(cxx::forward<T>(args)...)
  {}

  void init(Mword bits = 0)
  {
    reset();
    enable(bits);
  }

  bool available() const noexcept
  { return r.get_mmio_base() != 0; }

  Arm_mptimer mptimer() const noexcept
  {
    return Arm_mptimer(r.get_mmio_base() + 0x600);
  }
};

