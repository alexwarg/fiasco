
#pragma once

#include <irq_chip.h>
#include <irq_chip_generic.h>
#include <cascade_irq.h>
#include <mmio_register_block.h>
#include <warn.h>

class Gpio_eint_chip : public Irq_chip_gen, private Mmio_register_block
{
private:
  unsigned offs(Mword pin) const { return (pin >> 3) * 4; }

public:
  IRQ_CHIP_DBG_INFO("EI-Gpio");

  Gpio_eint_chip(Mword gpio_base, unsigned num_irqs)
    : Irq_chip_gen(num_irqs), _gpio_base(gpio_base)
  {}

  void mask(Mword pin)
  { Io::set<Mword>(1 << (pin & 7), _gpio_base + MASK + offs(pin)); }

  void ack(Mword pin)
  { Io::set<Mword>(1 << (pin & 7), _gpio_base + PEND + offs(pin)); }

  void mask_and_ack(Mword pin) { mask(pin); ack(pin); }

  void unmask(Mword pin)
  { Io::clear<Mword>(1 << (pin & 7), _gpio_base + MASK + offs(pin)); }

  void set_cpu(Mword, Cpu_number) {}
  int set_mode(Mword pin, Mode m)
  {
    unsigned v;

    if (m.wakeup())
      return -L4_err::EInval;

    if (!m.set_mode())
      return 0;

    switch (m.flow_type())
    {
      default:
      case Irq_chip::Mode::Trigger_level | Irq_chip::Mode::Polarity_low:  v = 0; break;
      case Irq_chip::Mode::Trigger_level | Irq_chip::Mode::Polarity_high: v = 1; break;
      case Irq_chip::Mode::Trigger_edge  | Irq_chip::Mode::Polarity_low:  v = 2; break;
      case Irq_chip::Mode::Trigger_edge  | Irq_chip::Mode::Polarity_high: v = 3; break;
      case Irq_chip::Mode::Trigger_edge  | Irq_chip::Mode::Polarity_both: v = 4; break;
    };

    Mword a = _gpio_base + INTCON + offs(pin);
    pin = pin % 8;
    v <<= pin * 4;
    Io::write<Mword>((Io::read<Mword>(a) & ~(7 << (pin * 4))) | v, a);

    return 0;
  }

  bool is_edge_triggered(Mword pin) const
  {
    unsigned v;
    Mword a = _gpio_base + INTCON + offs(pin);
    pin = pin % 8;
    v = (Io::read<Mword>(a) >> (pin * 4)) & 7;
    return v & 6;
  }

  unsigned pending() { return Io::read<Mword>(_gpio_base + 0xb08); }

  static void cascade_hit(Irq_base *_self, Upstream_irq const *u)
  {
    // this function calls some virtual functions that might be
    // ironed out
    Cascade_irq *self = nonull_static_cast<Cascade_irq*>(_self);
    Gpio_eint_chip *i = nonull_static_cast<Gpio_eint_chip*>(self->child());
    Upstream_irq ui(self, u);
    i->Gpio_eint_chip::irq(i->pending() - 8)->hit(&ui);
  }

private:
  enum {
    INTCON = 0x700,
    MASK   = 0x900,
    PEND   = 0xa00,
  };
  Mword _gpio_base;
};

class Gpio_wakeup_chip : public Irq_chip_gen, private Mmio_register_block
{
private:
  unsigned offs(Mword pin) const { return (pin >> 3) * 4; }

public:
  IRQ_CHIP_DBG_INFO("WU-GPIO");

  explicit Gpio_wakeup_chip(Address mmio_va)
  : Irq_chip_gen(32),
    Mmio_register_block(mmio_va),
    _wakeup(0)
  {}

  void mask(Mword pin)
  { modify<Mword>(1 << (pin & 7), 0, MASK + offs(pin)); }

  void ack(Mword pin)
  { modify<Mword>(1 << (pin & 7), 0, PEND + offs(pin)); }

  void mask_and_ack(Mword pin) { mask(pin); ack(pin); }

  void unmask(Mword pin)
  { modify<Mword>(0, 1 << (pin & 7), MASK + offs(pin)); }
  void set_cpu(Mword, Cpu_number) {}

  int set_mode(Mword pin, Mode m)
  {
    unsigned v;

    if (m.set_wakeup() && m.clear_wakeup())
      return -L4_err::EInval;

    if (m.set_wakeup())
      _wakeup |= 1 << pin;
    else if (m.clear_wakeup())
      _wakeup &= ~(1 << pin);

    if (!m.set_mode())
      return 0;

    switch (m.flow_type())
    {
      default:
      case Irq_chip::Mode::Trigger_level | Irq_chip::Mode::Polarity_low:  v = 0; break;
      case Irq_chip::Mode::Trigger_level | Irq_chip::Mode::Polarity_high: v = 1; break;
      case Irq_chip::Mode::Trigger_edge  | Irq_chip::Mode::Polarity_low:  v = 2; break;
      case Irq_chip::Mode::Trigger_edge  | Irq_chip::Mode::Polarity_high: v = 3; break;
      case Irq_chip::Mode::Trigger_edge  | Irq_chip::Mode::Polarity_both: v = 4; break;
    };

    Mword a = INTCON + offs(pin);
    pin = pin % 8;
    v <<= pin * 4;
    modify<Mword>(v, 7UL << (pin * 4), a);

    return 0;
  }

  bool is_edge_triggered(Mword pin) const
  {
    unsigned v;
    Mword a = INTCON + offs(pin);
    pin = pin % 8;
    v = (read<Mword>(a) >> (pin * 4)) & 7;
    return v & 6;
  }

  unsigned pending01() const // debug only
  {
    return read<Unsigned8>(PEND + 0) | (static_cast<unsigned>(read<Unsigned8>(PEND +  4)) << 8);
  }

  unsigned pending23() const
  {
    return read<Unsigned8>(PEND + 8) | (static_cast<unsigned>(read<Unsigned8>(PEND + 12)) << 8);
  }

  Mword mask23()
  { return read<Mword>(MASK + offs(16)) | (read<Mword>(MASK + offs(24)) << 8); }

  Unsigned32 _wakeup;

private:
  enum {
    INTCON = 0xe00,
    FLTCON = 0xe80,
    MASK   = 0xf00,
    PEND   = 0xf40,
  };
};


class Gpio_cascade_wu01_irq : public Irq_base
{
public:
  explicit Gpio_cascade_wu01_irq(Gpio_wakeup_chip *gc, unsigned pin)
  : _wu_gpio(gc), _pin(pin)
  { set_hit(&handler_wrapper<Gpio_cascade_wu01_irq>); }

  void switch_mode(bool) {}

  void handle(Upstream_irq const *u)
  {
    // checking pending reg as a debug thing
    if (!(_wu_gpio->pending01() & (1 << _pin)))
      WARN("WU-GPIO not pending %d\n", _pin);

    Upstream_irq ui(this, u);
    _wu_gpio->irq(_pin)->hit(&ui);
  }

private:
  Gpio_wakeup_chip *_wu_gpio;
  unsigned _pin;
};


class Gpio_cascade_wu23_irq : public Irq_base
{
public:
  explicit Gpio_cascade_wu23_irq(Gpio_wakeup_chip *gc)
  : _wu_gpio(gc)
  { set_hit(&handler_wrapper<Gpio_cascade_wu23_irq>); }

  void switch_mode(bool) {}

  void handle(Upstream_irq const *u)
  {
    Unsigned32 pending = (_wu_gpio->pending23() & ~_wu_gpio->mask23()) << 16;
    Upstream_irq ui(this, u);
    while (pending)
      {
        unsigned p = __builtin_ctz(pending);
        _wu_gpio->irq(p)->hit(&ui);
        pending &= ~(1 << p);
      }
  }

private:
  Gpio_wakeup_chip *_wu_gpio;
};


