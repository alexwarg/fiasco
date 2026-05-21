#pragma once

#include <mmio_register_block.h>
#include <types.h>
#include <config.h>

class Mct_timer : public Mmio_register_block
{
public:
  explicit Mct_timer(Address virt) : Mmio_register_block(virt) {}

  struct Reg { enum
  {
    Cfg   = 0x0,
    Cnt_l = 0x100,
    Cnt_u = 0x104,
    Tcon  = 0x240,
  }; };

  void start_free_running();
};

class Mct_core_timer : public Mmio_register_block
{
public:
  explicit Mct_core_timer(Address virt) : Mmio_register_block(virt) {}
  struct Reg { enum
  {
    L0          = 0x300,
    L1          = 0x400,

    L_TCNTB     = 0x00,
    L_TCNTO     = 0x04,
    L_ICNTB     = 0x08,
    L_ICNTO     = 0x0c,
    L_FRCNTB    = 0x10,
    L_FRCNTO    = 0x14,
    L_TCON      = 0x20,
    L_INT_CSTAT = 0x30,
    L_INT_ENB   = 0x34,
    L_WSTAT     = 0x40,
  }; };

  enum
  {
    Mct_freq = 24000000,
    Interval = Mct_freq / (1000000 / Config::Scheduler_granularity) / 2,
    Maxinterval_mct = (1U << 31) - 1,
    Maxinterval_us  = Maxinterval_mct / (Mct_freq / 1000000),
  };

  void set_interval(Mword interval)
  {
    write<Mword>((1 << 31) | interval, Reg::L_ICNTB);
    wstat_poll(2);
  }

  void configure();

  void acknowledge() const
  {
    write<Mword>(1, Reg::L_INT_CSTAT);
  }

private:
  void wstat_poll(unsigned val)
  {
    while ((read<Mword>(Reg::L_WSTAT) & val) == 0)
      ;
    write<Mword>(val, Reg::L_WSTAT);
  }

};

