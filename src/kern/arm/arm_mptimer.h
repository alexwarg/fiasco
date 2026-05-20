#pragma once

#include <types.h>
#include <mmio_register_block.h>

struct Arm_mptimer
{
  enum R
  {
    Load     = 0x0,
    Counter  = 0x4,
    Control  = 0x8,
    Int_stat = 0xc,
  };

  enum Control
  {
    _Prescaler = 0,

    Enable    = 1 << 0,
    Reload    = 1 << 1,
    Itenable  = 1 << 2,
    Prescaler = (_Prescaler & 0xff) << 8,
  };

  enum Int_stat
  {
    Event   = 1,
  };

  explicit Arm_mptimer(Address mmio_va)
  : r(mmio_va)
  {}

  Register_block<32> r;

  void ack() const noexcept
  {
    r[R::Int_stat] = Int_stat::Event;
  }

  Mword start_as_counter() noexcept
  {
    Mword v = ~0UL;
    r[R::Counter] = v;
    r[R::Control] = Control::Prescaler | Control::Reload
                    | Control::Enable;
    return v;
  }

  Mword stop_counter() noexcept
  {
    Mword v = r[R::Counter];
    r[R::Control] = 0;
    return v;
  }

  void init_periodic(Mword interval)
  {
    r[R::Load] = interval;
    r[R::Counter] = interval;
    r[R::Control] = Control::Prescaler | Control::Reload
      | Control::Enable | Control::Itenable;
    ack();
  }
};

