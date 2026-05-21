#pragma once

#include <irq_mgr.h>
#include <exynos_gpio_chip.h>
#include <exynos_ext_gic.h>
#include <irq_combiner.h>
#include <irq_entry.h>
#include <boot_alloc.h>
#include <kmem.h>
#include <pic-gic-helper.h>

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
  struct Irq_info
  {
    unsigned short chip;
    unsigned short irq;
  };

  struct Gpio_info
  {
    Address phys;
    unsigned short irq;
    unsigned short n_irqs;
  };

  struct Info
  {
    Pic_gic::Gic_info gic;
    unsigned long gic_offset = 0;
    unsigned gic_irqs;
    unsigned num_combiners;
    unsigned short const *c_irqs;
    Address wu_phys;
    Irq_info const *wu_irqs;
    unsigned n_gpio;
    Gpio_info gpio[4];
  };

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

  Irq exynos_irq(Irq_info const &i)
  {
    return Irq(_block[i.chip].chip, i.irq);
  }

  static void ext_gic_handler()
  {
    nonull_static_cast<Ext_gic *>(
        nonull_static_cast<Mgr_exynos *>(Irq_mgr::mgr)->_block[0].chip)->hit(nullptr);
  }

  explicit Mgr_exynos(Info const *info)
  {
    _nr_blocks = 3 + info->n_gpio;

    _block = Boot_alloced::allocate<Chip_block>(_nr_blocks);
    _block[0].sz = info->gic_irqs;

    if (info->gic_offset != 0)
      {
        _block[0].chip = new Boot_object<Ext_gic>(
            Kmem::mmio_remap(info->gic.cpu_phys, info->gic.cpu_size),
            Kmem::mmio_remap(info->gic.dist_phys, info->gic.dist_size),
            info->gic_offset);
        Arm_irqs::set_irq_handler(&ext_gic_handler);
      }
    else
      Pic_gic::create_gicv2(this, info->gic);

    _block[1].sz = info->num_combiners * 8;
    Combiner_chip *comb;
    _block[1].chip = comb = new Boot_object<Combiner_chip>(
        Kmem::mmio_remap(0x10440000, 0x1000), info->num_combiners);

    _block[2].sz = 32; // 32 wakup IRQs
    Gpio_wakeup_chip *wu;
    _block[2].chip = wu = new Boot_object<Gpio_wakeup_chip>(
        Kmem::mmio_remap(info->wu_phys, 0x1000));

    for (unsigned n = 0; n < info->n_gpio; ++n)
      {
        auto const &gp = info->gpio[n];
        auto &b = _block[3 + n];
        auto *g = _block[0].chip;

        b.sz = gp.n_irqs;
        b.chip = new Boot_object<Gpio_eint_chip>(Kmem::mmio_remap(gp.phys, 0x1000), gp.n_irqs);
        g->alloc(new Boot_object<Cascade_irq>(b.chip, Gpio_eint_chip::cascade_hit), gp.irq);
        g->unmask(gp.irq);
      }

    for (unsigned n = 0; n < info->num_combiners; ++n)
      {
        unsigned irq_num = info->c_irqs[n] + 32; // SPI + 32
        auto irq = chip(irq_num);
        irq.chip->alloc(new Boot_object<Combiner_cascade_irq>(n, comb), irq.pin);
        irq.chip->unmask(irq.pin);
      }

    for (unsigned n = 0; n < 16; ++n)
      {
        Irq irq = exynos_irq(info->wu_irqs[n]);
        irq.chip->alloc(new Boot_object<Gpio_cascade_wu01_irq>(wu, n), irq.pin);
        irq.chip->unmask(irq.pin);
      }

      {
        Irq irq = exynos_irq(info->wu_irqs[16]);
        irq.chip->alloc(new Boot_object<Gpio_cascade_wu23_irq>(wu), irq.pin);
        irq.chip->unmask(irq.pin);
      }
  }

  void init_ap(Cpu_number cpu, bool resume)
  {
    _block[0].chip->init_ap(cpu, resume);
  }

protected:
  unsigned _nr_blocks;
  unsigned _nr_irqs;
  Chip_block *_block;
};


