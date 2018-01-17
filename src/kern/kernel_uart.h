#pragma once

#include "config.h"

#if defined(CONFIG_SERIAL)
#include "uart.h"
#else
class Uart;
#endif

class Kernel_uart
{
public:
  enum Init_mode
  {
    Init_before_mmu,
    Init_after_mmu
  };

  Kernel_uart();
  static void pm_register();
  static void enable_rcv_irq();
  static bool init(Init_mode = Init_before_mmu);
  static bool init_for_mode(Init_mode init_mode);
  [[gnu::const]] static Uart *uart();
};

#if ! defined(CONFIG_SERIAL)
bool
Kernel_uart::init(Init_mode)
{ return false; }

inline
Kernel_uart::Kernel_uart()
{}

inline
void
Kernel_uart::enable_rcv_irq()
{}
#endif // CONFIG_SERIAL

