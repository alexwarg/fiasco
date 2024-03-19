#pragma once

#include <stddef.h>
#include "io_regblock.h"

namespace L4
{
  class Uart_iface
  {
  public:
    typedef unsigned Transfer_mode;
    typedef unsigned Baud_rate;

    Uart_iface() = default;
    Uart_iface(Uart_iface const &) = delete;
    Uart_iface(Uart_iface &&) = delete;
    Uart_iface &operator = (Uart_iface const &) = delete;
    Uart_iface &operator = (Uart_iface &&) = delete;
    virtual ~Uart_iface() = default;

    virtual bool startup(Io_register_block const *regs) = 0;
    virtual void set_base_rate(unsigned rate) = 0;

    virtual void shutdown() = 0;
    virtual bool change_mode(Transfer_mode m, Baud_rate r) = 0;
    virtual int get_char(bool blocking = true) const = 0;
    virtual int char_avail() const = 0;
    virtual int write(char const *s, unsigned long count) const = 0;
    virtual void irq_ack() = 0;
    virtual bool enable_rx_irq(bool = true) = 0;
    virtual Transfer_mode mode() const = 0;
    virtual Baud_rate rate() const = 0;
  };

  class Uart : public virtual Uart_iface
  {
  protected:
    unsigned _mode;
    unsigned _rate;
    Io_register_block const *_regs;

  public:
    void *operator new (size_t, void* a)
    { return a; }

  public:
    Uart()
    : _mode(~0U), _rate(~0U)
    {}

    void irq_ack() override {}

    bool enable_rx_irq(bool = true) override { return false; }
    Transfer_mode mode() const override { return _mode; }
    Baud_rate rate() const override { return _rate; }
  };
}
