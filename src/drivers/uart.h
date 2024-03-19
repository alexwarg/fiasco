#pragma once

#include "console.h"
#include <uart_base.h>

/**
 * Platform independent UART stub.
 */
class Uart : public Console, public virtual L4::Uart_iface
{
public:
  /**
   * Type UART transfer mode (Bits, Stopbits etc.).
   */
  typedef unsigned TransferMode;

  /**
   * Type for baud rate.
   */
  typedef unsigned BaudRate;

  /* These constants must be defined in the 
     arch part of the uart. To define them there
     has the advantage of most efficient definition
     for the hardware.

  static unsigned const PAR_NONE = xxx;
  static unsigned const PAR_EVEN = xxx;
  static unsigned const PAR_ODD  = xxx;
  static unsigned const DAT_5    = xxx;
  static unsigned const DAT_6    = xxx;
  static unsigned const DAT_7    = xxx;
  static unsigned const DAT_8    = xxx;
  static unsigned const STOP_1   = xxx;
  static unsigned const STOP_2   = xxx;

  static unsigned const MODE_8N1 = PAR_NONE | DAT_8 | STOP_1;
  static unsigned const MODE_7E1 = PAR_EVEN | DAT_7 | STOP_1;

  // these two values are to leave either mode
  // or baud rate unchanged on a call to change_mode
  static unsigned const MODE_NC  = xxx;
  static unsigned const BAUD_NC  = xxx;

  */
  enum
  {
    PAR_NONE = 0x00,
    PAR_EVEN = 0x18,
    PAR_ODD  = 0x08,
    DAT_5    = 0x00,
    DAT_6    = 0x01,
    DAT_7    = 0x02,
    DAT_8    = 0x03,
    STOP_1   = 0x00,
    STOP_2   = 0x04,

    MODE_8N1 = PAR_NONE | DAT_8 | STOP_1,
    MODE_7E1 = PAR_EVEN | DAT_7 | STOP_1,

    // these two values are to leave either mode
    // or baud rate unchanged on a call to change_mode
    MODE_NC  = 0x1000000,
    BAUD_NC  = 0x1000000,
  };


public:
  Uart() : Console(DISABLED) {}

  int irq() const
  {
    return _irq;
  }

  Mword get_attributes() const override
  {
    return UART | IN | OUT;
  }

protected:
  bool startup(L4::Io_register_block const *reg, int irq, Unsigned32 base_baud)
  {
    _irq = irq;
    set_base_rate(base_baud);

    if (!static_cast<Uart_iface *>(this)->startup(reg))
      return false;

    add_state(ENABLED);
    return true;
  }

private:
  int _irq = -1;
};

