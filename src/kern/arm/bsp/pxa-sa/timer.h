#pragma once

#include <mmio_register_block.h>
#include <irq_chip.h>
#include <types.h>

struct Timer : Mmio_register_block
{
public:
  static unsigned irq() { return 26; }

  static Irq_chip::Mode irq_mode()
  { return Irq_chip::Mode::F_raising_edge; }

  Timer();

  static void init(Cpu_number)
  {
    _timer.construct();
  }

  static void acknowledge()
  {
    _timer->write<Mword>(0, OSCR);
    _timer->write<Mword>(1, OSSR); // clear all status bits
  }

  static void enable()
  {}

  static void update_timer(Unsigned64 /*wakeup*/)
  {}

private:
  enum {
    OSMR0 = 0x00,
    OSMR1 = 0x04,
    OSMR2 = 0x08,
    OSMR3 = 0x0c,
    OSCR  = 0x10,
    OSSR  = 0x14,
    OWER  = 0x18,
    OIER  = 0x1c,

    Timer_diff = (36864 * Config::Scheduler_granularity) / 10000, // 36864MHz*1ms
  };

  static Static_object<Timer> _timer;
#if 0
  static inline Unsigned64 timer_to_us(Unsigned32 cr);
  static inline Unsigned64 us_to_timer(Unsigned64 us);
  inline void ack();
#endif
};
