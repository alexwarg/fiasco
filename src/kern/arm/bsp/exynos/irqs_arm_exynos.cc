
#include <irqs-arm-exynos.h>
#include <pic-gic-helper.h>
#include <gic.h>
#include <gic_v2.h>
#include <initcalls.h>
#include <mmio_register_block.h>
#include <irq_mgr.h>
#include <irq_chip.h>
#include <irq_chip_generic.h>
#include <boot_alloc.h>
#include <io.h>
#include <warn.h>
#include <pic.h>
#include <kmem.h>
#include <ipi.h>

#include <platform.h>

#include <globalconfig.h>


class Mgr_exynos : public Irq_mgr_dyn
{
protected:
  struct Chip_block
  {
    unsigned sz;
    Irq_chip_icu *chip;
  };

  void calc_nr_irq()
  {
    _nr_irqs = 0;
    for (unsigned i = 0; i < _nr_blocks; ++i)
      _nr_irqs += _block[i].sz;
  }

public:
  unsigned nr_irqs() const { return _nr_irqs; }
  unsigned nr_msis() const { return 0; }

  int add_chip(int base, Irq_chip_icu *chip, int pins = -1) override
  {
    (void) pins;
    if (base != 0)
      return -E_range;

    _block[0].chip = chip;
    return 0;
  }

protected:
  unsigned _nr_blocks;
  unsigned _nr_irqs;
  Chip_block *_block;
};


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

  explicit Gpio_wakeup_chip(Address physbase)
  : Irq_chip_gen(32),
    Mmio_register_block(Kmem::mmio_remap(physbase, 0x1000)),
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

class Combiner_chip : public Irq_chip_gen, private Mmio_register_block
{
public:
  IRQ_CHIP_DBG_INFO("Comb");

  enum
  {
    Enable_set    = 0,
    Enable_clear  = 4,
    Status        = 12,
  };

  enum
  {
    No_pending = ~0UL,
  };

  Mword offset(unsigned irq) const { return (irq >> 2) * 0x10; }

  static unsigned shift(int irq)
  { return (irq % 4) * 8; }

  static Mword bytemask(int irq)
  { return 0xffUL << shift(irq); }

  Mword status(int irq) const
  { return read<Mword>(offset(irq) + Status) & bytemask(irq); }

  void mask(Mword i)
  { write<Mword>(1UL << (i & 31), offset(i / 8) + Enable_clear); }

  void mask_and_ack(Mword i)
  { Combiner_chip::mask(i); }

  void ack(Mword) {}

  void set_cpu(Mword, Cpu_number) {}

  int set_mode(Mword, Mode)
  { return 0; }

  bool is_edge_triggered(Mword) const
  { return false; }

  void unmask(Mword i)
  { write<Mword>(1UL << (i & 31), offset(i / 8) + Enable_set); }

  void init_irq(int irq) const
  { write<Mword>(bytemask(irq), offset(irq) + Enable_clear); }

  Unsigned32 pending(unsigned cnr)
  {
    unsigned v = status(cnr) >> shift(cnr);
    if (v)
      return (cnr * 8) + __builtin_ctz(v);
    return No_pending;
  }

  int num_combiner_chips() const
  {
    if (Platform::is_4210())
      return Platform::gic_int() ? 54 : 16;
    if (Platform::is_4412())
      return 20;
    if (Platform::is_5250() || Platform::is_5410())
      return 32;
    assert(0);
    return 0;
  }

  Combiner_chip()
  : Irq_chip_gen(num_combiner_chips() * 8),
    Mmio_register_block(Kmem::mmio_remap(Mem_layout::Irq_combiner_phys_base,
                                         0x1000))
  {
    // 0..39, 51, 53
    if (Platform::gic_int())
      {
        for (int i = 0; i < 40; ++i)
          init_irq(i);
        init_irq(51);
        init_irq(53);
      }
    else
      {
        const int num = num_combiner_chips();
        for (int i = 0; i < num; ++i)
          init_irq(i);
      }
  }

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


class Gpio_cascade_xab_irq : public Irq_base
{
public:
  explicit Gpio_cascade_xab_irq(Gpio_eint_chip *g, unsigned special = 0)
  : _eint_gc(g), _special(special)
  { set_hit(&handler_wrapper<Gpio_cascade_xab_irq>); }

  void switch_mode(bool) {}

  void handle(Upstream_irq const *u)
  {
    Mword p = _eint_gc->pending();
    Upstream_irq ui(this, u);
    if (1)
      {
        int grp = (p >> 3) & 0x1f;
        int pin = p & 7;

        if (_special == 1)
          {
            if (grp > 7)
              grp += 5;
          }
        else if (_special == 2)
          grp += 2;

        _eint_gc->irq((grp - 1) * 8 + pin)->hit(&ui);
      }
    else
      _eint_gc->irq(p - 8)->hit(&ui);
  }


private:
  Gpio_eint_chip *_eint_gc;
  unsigned _special;
};

class Combiner_cascade_irq : public Irq_base
{
public:
  Combiner_cascade_irq(unsigned nr, Combiner_chip *chld)
  : _combiner_nr(nr), _child(chld)
  { set_hit(&handler_wrapper<Combiner_cascade_irq>); }

  void switch_mode(bool) {}
  unsigned irq_nr_base() const { return _combiner_nr * 8; }

  void handle(Upstream_irq const *u)
  {
    Unsigned32 num = _child->pending(_combiner_nr);
    Upstream_irq ui(this, u);

    if (num != Combiner_chip::No_pending)
      _child->irq(num)->hit(&ui);
  }

private:
  unsigned _combiner_nr;
  Combiner_chip *_child;
};


class Mgr_int_ex4 : public Mgr_exynos
{
public:
  Mgr_int_ex4();

  void set_cpu(Mword irqnum, Cpu_number cpu) const
  {
    // this handles only the MCT_L[01] timers
    if (   irqnum == 379  // MCT_L1: Combiner 35:3
        || irqnum == 504) // MCT_L0: Combiner 51:0
      _block[0].chip->set_cpu(32 + (irqnum - 96) / 8, cpu);
    else
      WARNX(Warning, "IRQ%ld: ignoring CPU setting (%d).\n",
            irqnum, cxx::int_value<Cpu_number>(cpu));
  }

  Irq chip(Mword irqnum) const
  {
    Mword origirq = irqnum;

    for (unsigned i = 0; i < _nr_blocks; ++i)
      {
        if (irqnum < _block[i].sz)
          return Irq(_block[i].chip, irqnum);

        irqnum -= _block[i].sz;
      }

    printf("KERNEL: exynos-irq: Invalid irqnum=%ld\n", origirq);
    return Irq();
  }
};

Mgr_int_ex4::Mgr_int_ex4()
{
  Combiner_chip *_cc
    = new Boot_object<Combiner_chip>();
  Gpio_wakeup_chip *_wu_gc
    = new Boot_object<Gpio_wakeup_chip>(0x11000000);
  Gpio_eint_chip *_ei_gc1
    = new Boot_object<Gpio_eint_chip>(Kmem::mmio_remap(0x11400000,
                                                       0x1000), 16 * 8);
  Gpio_eint_chip *_ei_gc2
    = new Boot_object<Gpio_eint_chip>(Kmem::mmio_remap(0x11000000,
                                                       0x1000), (29 - 21 + 1) * 8);

  static Chip_block soc[] = {
    { 96,                 nullptr },
    { 54 * 8,             _cc },
    { 32,                 _wu_gc },
    { _ei_gc1->nr_irqs(), _ei_gc1 },
    { _ei_gc2->nr_irqs(), _ei_gc2 },
  };

  _block = soc;
  _nr_blocks = sizeof(soc) / sizeof(soc[0]);

  Pic_gic::create_gicv2(this, Pic_gic::Gic_info
    {
      .version = 2, .primary = true, .offset = 0,
      .dist_phys = 0x10501000, .dist_size = 0x1000,
      .cpu_phys =  0x10500100,  .cpu_size =  0x100,
    });

  Irq_chip_icu *gic = _block[0].chip;

  // Combiners
  for (unsigned i = 0; i < 40; ++i)
    {
      gic->alloc(new Boot_object<Combiner_cascade_irq>(i, _cc), i + 32);
      gic->unmask(i + 32);
    }
  gic->alloc(new Boot_object<Combiner_cascade_irq>(51, _cc), 51 + 32);
  gic->unmask(51 + 32);
  gic->alloc(new Boot_object<Combiner_cascade_irq>(53, _cc), 53 + 32);
  gic->unmask(53 + 32);

  // GPIO-wakeup0-3 goes to GIC
  gic->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, 0), 72); gic->unmask(72);
  gic->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, 1), 73); gic->unmask(73);
  gic->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, 2), 74); gic->unmask(74);
  gic->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, 3), 75); gic->unmask(75);

  // GPIO-wakeup4-7 -> comb37:0-3
  for (unsigned i = 0; i < 4; ++i)
    {
      _cc->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, 4 + i), 8 * 37 + i);
      _cc->unmask(8 * 37 + i);
    }

  // GPIO-wakeup8-15 -> COMB:38:0-7
  for (unsigned i = 0; i < 8; ++i)
    {
      _cc->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, 8 + i), 8 * 38 + i);
      _cc->unmask(8 * 38 + i);
    }

  // GPIO-wakeup16-31: COMP:39:0
  _cc->alloc(new Boot_object<Gpio_cascade_wu23_irq>(_wu_gc), 8 * 39 + 0);
  _cc->unmask(8 * 39 + 0);

  // xa
  _cc->alloc(new Boot_object<Gpio_cascade_xab_irq>(_ei_gc1), 8 * 24 + 1);
  _cc->unmask(8 * 24 + 1);

  // xb
  _cc->alloc(new Boot_object<Gpio_cascade_xab_irq>(_ei_gc2), 8 * 24 + 0);
  _cc->unmask(8 * 24 + 0);

  calc_nr_irq();
}

class Mgr_ex5 : public Mgr_exynos
{
public:
  Mgr_ex5();

  Irq chip(Mword irqnum) const
  {
    Mword origirq = irqnum;

    for (unsigned i = 0; i < _nr_blocks; ++i)
      {
        if (irqnum < _block[i].sz)
          return Irq(_block[i].chip, irqnum);

        irqnum -= _block[i].sz;
      }

    printf("KERNEL: exynos-irq: Invalid irqnum=%ld\n", origirq);
    return Irq();
  }
};

Mgr_ex5::Mgr_ex5()
{
  Combiner_chip *_cc
    = new Boot_object<Combiner_chip>();

  Gpio_wakeup_chip *_wu_gc
    = new Boot_object<Gpio_wakeup_chip>(0x11400000);

  Gpio_eint_chip *_ei_gc1
    = new Boot_object<Gpio_eint_chip>(Kmem::mmio_remap(0x11400000,
                                                       0x1000), 13 * 8);
  Gpio_eint_chip *_ei_gc2
    = new Boot_object<Gpio_eint_chip>(Kmem::mmio_remap(0x11000000,
                                                       0x1000),  8 * 8);
  Gpio_eint_chip *_ei_gc3
    = new Boot_object<Gpio_eint_chip>(Kmem::mmio_remap(0x10d10000,
                                                       0x1000),  5 * 8);
  Gpio_eint_chip *_ei_gc4
    = new Boot_object<Gpio_eint_chip>(Kmem::mmio_remap(0x03860000,
                                                       0x1000),  1 * 8);
  // 5250
  //  - part1: ext-int 0x700: 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 30
  //                   0xe00: 40, 41, 42, 43
  //  - part2: ext-int 0x700: 14, 15, 16, 17, 18, 19, 20, 21, 22
  //  - part3: ext-int 0x700: 60, 61, 62, 63, 64
  //  - part4: ext-int 0x700: 50

  _block = Boot_alloced::allocate<Chip_block>(7);
  _nr_blocks = 7;

  _block[0].sz = Platform::is_5410() ? 256U : 160U;
  _block[1] = Chip_block{32 * 8, _cc};
  _block[2] = Chip_block{32    , _wu_gc};
  _block[3] = { _ei_gc1->nr_irqs(),                _ei_gc1 };
  _block[4] = { _ei_gc2->nr_irqs(),                _ei_gc2 };
  _block[5] = { _ei_gc3->nr_irqs(),                _ei_gc3 };
  _block[6] = { _ei_gc4->nr_irqs(),                _ei_gc4 };

  Pic_gic::create_gicv2(this, Pic_gic::Gic_info{
      .version = 2, .primary = true, .offset = 0,
      .dist_phys  = 0x10481000, .dist_size = 0x1000,
      .cpu_phys   = 0x10482000, .cpu_size  = 0x1000,
      .cpu_h_phys = 0x10484000, .cpu_h_size = 0x2000,
      .cpu_v_phys = 0x10486000, .cpu_v_size = 0x1000,
      });

  Irq_chip_icu *g = _block[0].chip;

  // Combiners
  for (unsigned i = 0; i < 32; ++i)
    {
      g->alloc(new Boot_object<Combiner_cascade_irq>(i, _cc), i + 32);
      g->unmask(i + 32);
    }

  _cc->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, 0), 8 * 23 + 0);
  _cc->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, 1), 8 * 24 + 0);
  for (int i = 25, nr = 2; i < 32; ++i, nr += 2)
    {
      _cc->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, nr + 0), 8 * i + 0);
      _cc->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, nr + 1), 8 * i + 1);
    }

  // GPIO-wakeup16-31: GIC:32+32
  g->alloc(new Boot_object<Gpio_cascade_wu23_irq>(_wu_gc), 64);
  g->unmask(64);

  if (0)
    {
      // xa GIC:32+47
      g->alloc(new Boot_object<Gpio_cascade_xab_irq>(_ei_gc1), 79);
      g->unmask(79);

      // xb GIC:32+46
      g->alloc(new Boot_object<Gpio_cascade_xab_irq>(_ei_gc2), 78);
      g->unmask(78);
    }


  calc_nr_irq();
}


#ifdef CONFIG_PF_EXYNOS4
#ifdef CONFIG_PF_EXYNOS_EXTGIC

struct Ext_gic : Gic
{
  IRQ_CHIP_DBG_INFO("Ext GIC");

  Per_cpu_array<Static_object<Gic_v2>> g;
  Gic_v2 *current() { return g[current_cpu()]; }
  Gic_v2 *master() { return g[Cpu_number::boot_cpu()]; }
  Gic_v2 const *current() const { return g[current_cpu()]; }

  Ext_gic(Address cpu_base, Address dist_base, int nr_irqs_override = -1)
  {
    g[Cpu_number::boot_cpu()].construct(cpu_base, dist_base, nullptr);
    master()->init_gic(nr_irqs_override);
  }

  void ack(Mword pin) override
  { current()->Gic_v2::ack(pin); }

  void mask_and_ack(Mword pin) override
  { current()->Gic_v2::mask_and_ack(pin); }

  void set_cpu(Mword pin, Cpu_number cpu) override
  { g[cpu]->Gic_v2::set_cpu(pin, cpu); }

  void softint_cpu(Cpu_number target, unsigned m) override
  { current()->Gic_v2::softint_cpu(target, m); }

  void softint_bcast(unsigned m) override
  { current()->Gic_v2::softint_bcast(m); }

  void softint_phys(unsigned m, Unsigned64 target) override
  { current()->Gic_v2::softint_phys(m, target); }

  void init_ap(Cpu_number cpu, bool resume) override
  { g[cpu]->Gic_v2::init_ap(cpu, resume); }

  unsigned gic_version() const override
  { return 2; }

  void hit(Upstream_irq const *ui)
  {
    Unsigned32 num = current()->pending();

    // INTIDs 1020 - 1023 are spurious on GIC v2 and v3 and do not need an EOI
    if (EXPECT_FALSE((num & 0xfffffffc) == 0x3fc))
      return;

    handle_irq<Ext_gic>(num, ui);
  }

  void disable_locked(unsigned irq)
  { current()->Gic_v2::disable_locked(irq); }

  void enable_locked(unsigned irq)
  { current()->Gic_v2::enable_locked(irq); }

  void set_pending_irq(unsigned idx, Unsigned32 val)
  {
    current()->Gic_v2::set_pending_irq(idx, val);
  }

  void mask(Mword pin) override
  {
    assert (cpu_lock.test());
    disable_locked(pin);
  }

  void unmask(Mword pin) override
  {
    assert (cpu_lock.test());
    enable_locked(pin);
  }

  int set_mode(Mword pin, Mode m) override
  {
    return current()->Gic_v2::set_mode(pin, m);
  }

  bool is_edge_triggered(Mword pin) const override
  {
    return current()->Gic_v2::is_edge_triggered(pin);
  }

};

class Mgr_ext : public Mgr_exynos
{
public:
  Mgr_ext();

  /**
   * \pre must run on the CPU given in \a cpu.
   */
  void set_cpu(Mword irqnum, Cpu_number cpu) const
  {
    if (!Platform::is_4412() && irqnum == 80)  // MCT_L1
      gic.set_cpu(80, cpu);
    else
      WARNX(Warning, "IRQ%ld: ignoring CPU setting (%d).\n", irqnum,
            cxx::int_value<Cpu_number>(cpu));
  }

  Irq chip(Mword irqnum) const
  {
    Mword origirq = irqnum;

    for (unsigned i = 0; i < _nr_blocks; ++i)
      {
        if (irqnum < _block[i].sz)
          {
#if 0
            if (i == 0) // some special handling in GIC block
              if (!Platform::is_4412())
                if (irqnum == 80 && Config::Max_num_cpus > 1) // MCT_L1 goes to CPU1
                  return Irq(gic.cpu(Cpu_number(1)), irqnum);
#endif
            return Irq(_block[i].chip, irqnum);
          }

        irqnum -= _block[i].sz;
      }

    printf("KERNEL: exynos-irq: Invalid irqnum=%ld\n", origirq);
    return Irq();
  }

  static void exynos_irq_handler()
  {
    nonull_static_cast<Mgr_ext *>(Irq_mgr::mgr)->gic.hit(nullptr);
  }

  mutable Ext_gic gic;
};

Mgr_ext::Mgr_ext()
  : gic(Kmem::mmio_remap(0x10480000, Gic_cpu_v2::Size),
        Kmem::mmio_remap(0x10490000, Gic_dist::Size))
{
  Combiner_chip *_cc
    = new Boot_object<Combiner_chip>();
  Gpio_wakeup_chip *_wu_gc
    = new Boot_object<Gpio_wakeup_chip>(0x11000000);
  Gpio_eint_chip *_ei_gc1
    = new Boot_object<Gpio_eint_chip>(Kmem::mmio_remap(0x11400000,
                                                       0x1000), 18 * 8);
  Gpio_eint_chip *_ei_gc2
    = new Boot_object<Gpio_eint_chip>(Kmem::mmio_remap(0x11000000,
                                                       0x1000), 14 * 8);

  auto g = &gic;
  // Combiners
  for (unsigned i = 0; i < 16; ++i)
    {
      g->alloc(new Boot_object<Combiner_cascade_irq>(i, _cc), i + 32);
      g->unmask(i + 32);
    }

  if (Platform::is_4412())
    {
      g->alloc(new Boot_object<Combiner_cascade_irq>(16, _cc), 139); g->unmask(139);
      g->alloc(new Boot_object<Combiner_cascade_irq>(17, _cc), 140); g->unmask(140);
      g->alloc(new Boot_object<Combiner_cascade_irq>(18, _cc), 80);  g->unmask(80);
      g->alloc(new Boot_object<Combiner_cascade_irq>(19, _cc), 74);  g->unmask(74);
    }

  // GPIO-wakeup0-15 goes to GIC
  for (unsigned i = 0; i < 16; ++i)
    {
      g->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, i), i + 48);
      g->unmask(i + 48);
    }

  // GPIO-wakeup16-31: GIC:32+32
  g->alloc(new Boot_object<Gpio_cascade_wu23_irq>(_wu_gc), 64);
  g->unmask(64);

  // xa GIC:32+47
  g->alloc(new Boot_object<Gpio_cascade_xab_irq>(_ei_gc1, Platform::is_4412() ? 1 : 0), 79);
  g->unmask(79);

  // xb GIC:32+46
  g->alloc(new Boot_object<Gpio_cascade_xab_irq>(_ei_gc2, Platform::is_4412() ? 2 : 0), 78);
  g->unmask(78);


  // 4210
  //  - part1: ext-int 0x700: 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
  //  - part2: ext-int 0x700: 21, 22, 23, 24, 25, 26, 27, 28, 29
  //                   0xe00: wu1, wu2, wu3, wu4
  //  - part3: nix

  static Chip_block soc4210[] = {
    { 160,               g },
    { 16 * 8,            _cc },
    { 32,                _wu_gc },
    { 16 * 8,            _ei_gc1 },
    { (29 - 21 + 1) * 8, _ei_gc2 },
  };

  // 4412
  //  - part1: ext-int 0x700: 1, 2, 3, 4, 5, 6, 7, 13, 14, 15, 16, 21, 22
  //  - part2: ext-int 0x700: 23, 24, 25, 26, 27, 28, 29, 8, 9, 10, 11, 12
  //                   0xe00: 40, 41, 42, 43
  //  - part3: ext-int 0x700: 50
  //  - part4: ext-int 0x700: 30, 31, 32, 33, 34

  static Chip_block soc4412[] = {
    { 160,               g },
    { 20 * 8,            _cc },
    {  4 * 8,            _wu_gc },
    { 18 * 8,            _ei_gc1 },
    { 14 * 8,            _ei_gc2 },
    //{  1 * 8,            _ei_gc3 }, // Do not know upstream IRQ-num :(
    //{  5 * 8,            _ei_gc4 }, // Do not know upstream IRQ-num :(
  };

  if (Platform::is_4412())
    {
      _block = soc4412;
      _nr_blocks = sizeof(soc4412) / sizeof(soc4412[0]);
    }
  else
    {
      _block = soc4210;
      _nr_blocks = sizeof(soc4210) / sizeof(soc4210[0]);
    }

  calc_nr_irq();
}


FIASCO_INIT
void Pic::init()
{
  Mgr_ext *m = new Boot_object<Mgr_ext>();
  Irq_mgr::mgr = m;
  Gic::set_irq_handler(&Mgr_ext::exynos_irq_handler);
}

void
Irqs_arm_exynos::reinit(Cpu_number cpu)
{
  assert (cpu == current_cpu());
  //nonull_static_cast<Mgr_ext *>(Irq_mgr::mgr)->gic.init(true, 96);
}

class Check_irq0 : public Irq_base
{
public:
  Check_irq0() { set_hit(&hndl); }
  static void hndl(Irq_base *, Upstream_irq const *)
  {
    printf("IRQ0 appeared on CPU%d\n",
           cxx::int_value<Cpu_number>(current_cpu()));
  }
private:
  void switch_mode(bool) {}
};

DEFINE_PER_CPU static Per_cpu<Static_object<Check_irq0> > _check_irq0;


void Pic::init_ap(Cpu_number cpu, bool resume)
{
  Static_object<Gic_v2> &g = nonull_static_cast<Mgr_ext *>(Irq_mgr::mgr)->gic.g[cpu];
  Gic_v2 *g0 = nonull_static_cast<Mgr_ext *>(Irq_mgr::mgr)->gic.master();
  if (!resume)
    {
      unsigned long stride;
      unsigned phys_cpu = cxx::int_value<Cpu_phys_id>(Cpu::cpus.cpu(cpu).phys_id());
      if (Platform::is_4412())
        {
          assert(cpu > Cpu_number(0));
          assert(cpu < Cpu_number(4));
          stride = 0x4000;
        }
      else
        {
          assert (cpu == Cpu_number(1));
          assert (Cpu::cpus.cpu(cpu).phys_id() == Cpu_phys_id(1));
          stride = 0x8000;
        }

      g.construct(
          Kmem::mmio_remap(Mem_layout::Gic_cpu_ext_cpu0_phys_base + phys_cpu * stride,
                           0x1000),
          Kmem::mmio_remap(Mem_layout::Gic_dist_ext_cpu0_phys_base + phys_cpu * stride,
                           0x1000),
          g0);
    }

  g->init_ap(cpu, resume);

#if 0
  if (!resume &&)
    {
      // This is a debug facility as we've been seeing IRQ0
      // happening under (non-usual) high load
      _check_irq0.cpu(cpu).construct();
      g->alloc(_check_irq0.cpu(cpu), 0);
    }
#endif
}


#else // CONFIG_PF_EXYNOS_EXTGIC

FIASCO_INIT
void Pic::init()
{
  Irq_mgr::mgr = new Boot_object<Mgr_int_ex4>();
}

void Pic::init_ap(Cpu_number cpu, bool resume)
{
  Gic::primary->init_ap(cpu, resume);
}

#endif // CONFIG_PF_EXYNOS_EXTGIC

#ifdef CONFIG_ARM_EM_TZ
void
Irqs_arm_exynos::set_pending_irq(unsigned group32num, Unsigned32 val)
{
  Gic::primary->set_pending_irq(group32num, val);
}
#endif

#else // CONFIG_PF_EXYNOS4

FIASCO_INIT
void Pic::init()
{
  Irq_mgr::mgr = new Boot_object<Mgr_ex5>();
}

void Pic::init_ap(Cpu_number cpu, bool resume)
{
  Gic::primary->init_ap(cpu, resume);
}

void
Irqs_arm_exynos::reinit(Cpu_number cpu)
{
  assert (cpu == current_cpu());
  //nonull_static_cast<Mgr_ext *>(Irq_mgr::mgr)->gic.init(true, 96);
}

#endif // CONFIG_PF_EXYNOS4

