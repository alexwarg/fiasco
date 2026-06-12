#pragma once

#include <mmio_register_block.h>
#include <irq_chip.h>

class Timer_arm_sunxi
{
public:
  static unsigned irq() { return 54; }

  static Irq_chip::Mode irq_mode()
  { return Irq_chip::Mode::F_raising_edge; }

  static void enable()
  {}

  static void update_timer(Unsigned64 /*wakeup*/)
  {}

  static void init(Cpu_number);

  static void acknowledge()
  {
    _timer->write<Mword>(1 << Tmr::Timer_nr, Tmr::TMR_IRQ_STA_REG);
  }

private:
  class Tmr : public Mmio_register_block
  {
  public:
    enum {
      TMR_IRQ_EN_REG      = 0x0,
      TMR_IRQ_STA_REG     = 0x4,

      TMRx_base           = 0x10,
      TMRx_offset         = 0x10,
      TMRx_CTRL_REG       = 0x0,
      TMRx_INTV_VALUE_REG = 0x4,
      TMRx_CUR_VALUE_REG  = 0x8,

      Timer_nr = 0,
    };

    explicit Tmr(void *a) : Mmio_register_block(a) {}

    static Address addr(unsigned reg)
    { return TMRx_base + Timer_nr * TMRx_offset + reg; }
  };

private:
  static Static_object<Tmr> _timer;
};
