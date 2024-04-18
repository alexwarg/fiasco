#pragma once

#include <mmio_register_block.h>

class Timer_omap_gentimer : private Mmio_register_block
{
public:
  enum
  {
    TIDR          = 0x00, // ID
    TIOCP_CFG     = 0x10, // config
    EOI           = 0x20,
    IRQSTATUS     = 0x28,
    IRQENABLE_SET = 0x2c,
    IRQWAKEEN     = 0x34,
    TCLR          = 0x38,
    TCRR          = 0x3c,
    TLDR          = 0x40,
  };

  Timer_omap_gentimer();

  void acknowledge()
  {
    write<Mword>(2, IRQSTATUS);
    write<Mword>(0, EOI);
  }
};

