#pragma once

#include <mmio_register_block.h>

class Timer_omap_1mstimer : private Mmio_register_block
{
public:
  explicit Timer_omap_1mstimer(bool f_32khz);

  void acknowledge()
  {
    write<Mword>(2, TISR);
  }


private:
  enum {
    TIDR      = 0x000, // IP revision code
    TIOCP_CFG = 0x010, // config
    TISTAT    = 0x014, // non-interrupt status
    TISR      = 0x018, // pending interrupts
    TIER      = 0x01c, // enable/disable of interrupt events
    TWER      = 0x020, // wake-up features
    TCLR      = 0x024, // optional features
    TCRR      = 0x028, // internal counter
    TLDR      = 0x02c, // timer load value
    TTGR      = 0x030, // trigger reload by writing
    TWPS      = 0x034, // write-posted pending
    TMAR      = 0x038, // compare value
    TCAR1     = 0x03c, // first capture value of the counter
    TCAR2     = 0x044, // second capture value of the counter
    TPIR      = 0x048, // positive inc, gpt1, 2 and 10 only
    TNIR      = 0x04C, // negative inc, gpt1, 2 and 10 only
  };
};

