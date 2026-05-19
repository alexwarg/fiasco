#pragma once

#include <irq_chip_generic.h>
#include <mmio_register_block.h>
#include <globalconfig.h>

#ifdef CONFIG_MP

#include "ipi_control.h"

class Gic_ipi_control : public Ipi_control
{
protected:
  unsigned _ipi_base;
  unsigned _cpu_int_ipi = 6;
};

#else // CONFIG_MP
class Gic_ipi_control {};
#endif // CONFIG_MP

class Gic : public Irq_chip_gen, public Gic_ipi_control
{
public:
  IRQ_CHIP_DBG_INFO("GIC");

  enum
  {
    Size = 0x20000,
  };

  Gic(Address mmio, unsigned cpu_int);

  void unmask(Mword pin) override
  {
    mod_mask(pin, false, Sh_smask);
  }

  void mask(Mword pin) override
  {
    mod_mask(pin, true, Sh_rmask);
  }

  void mask_and_ack(Mword pin) override
  {
    mod_mask(pin, true, Sh_rmask);
  }

  void ack(Mword) override
  {}

  int set_mode(Mword pin, Mode mode) override;

  bool is_edge_triggered(Mword pin) const override
  {
    return sh_irq_bit(pin) & _r[sh_irq_reg(Sh_trigger, pin)];
  }

  void set_cpu(Mword pin, Cpu_number cpu) override;

  unsigned pending()
  {
    // We might also need to check that we're on the proper CPU but
    // lets postpone that until it is actually required
    for (unsigned i = 0, o = 0; i < nr_irqs(); o += Reg_bytes, i += Reg_width)
      if (_enabled_map[i / Reg_width])
        {
          Reg_type v = _r[Sh_pend + o] & _enabled_map[i / Reg_width];
          if (v)
            return i + __builtin_ffs(v) - 1;
        }

    return ~0U;
  }

#ifdef CONFIG_MP
  void send_ipi(Cpu_number to, Ipi *) override;
  void ack_ipi(Cpu_number cpu) override;
  void init_ipis(Cpu_number cpu, Irq_base *irq) override;
#endif // CONFIG_MP

private:
  typedef Mword Reg_type;
  enum : unsigned long
  {
    Reg_bytes = sizeof(Reg_type),
    Reg_width = Reg_bytes * 8
  };

  enum : Address
  {
    Core_local = 0x8000,
    Core_other = Core_local + 0x4000,
    User_visible = 0x10000,

    Sh_config       = 0x0000,
    Sh_counter_lo   = 0x0010,
    Sh_counter_hi   = 0x0014,
    Sh_revision_id  = 0x0020,
    Sh_int_avail    = 0x0024, // .. incl. 0x40
    Sh_gid_config   = 0x0080, // .. incl. 0x9c
    Sh_pol          = 0x0100, // .. incl. 0x11c
    Sh_trigger      = 0x0180, // .. incl. 0x19c
    Sh_dual_edge    = 0x0200, // .. incl. 0x21c
    Sh_wedge        = 0x0280,
    Sh_rmask        = 0x0300, // .. incl. 0x31c
    Sh_smask        = 0x0380, // .. incl. 0x39c
    Sh_mask         = 0x0400, // .. incl. 0x41c
    Sh_pend         = 0x0480, // .. incl. 0x49c
    Sh_map_pin      = 0x0500, // .. incl. 0x8fc
    Sh_map_vpe      = 0x2000, // .. incl. 0x3fe4

    Core_ctl         = 0x0000,
    Core_pend        = 0x0004,
    Core_mask        = 0x0008,
    Core_rmask       = 0x000c,
    Core_smask       = 0x0010,
    Core_wd_map      = 0x0040,
    Core_compare_map = 0x0044,
    Core_timer_map   = 0x0048,
    Core_fdc_map     = 0x004c,
    Core_prefctr_map = 0x0050,
    Core_swint0_map  = 0x0054,
    Core_swint1_map  = 0x0058,
    Core_other_addr  = 0x0080,
    Core_ident       = 0x0088,
    Core_wd_config0  = 0x0090,
    Core_wd_count0   = 0x0094,
    Core_wd_initial0 = 0x0098,
    Core_compare_lo  = 0x00a0,
    Core_compare_hi  = 0x00a4,
    Core_coffset     = 0x0200,
    Core_dint_part   = 0x3000,
    Core_brk_part    = 0x3080,
  };

  enum {
    Cpu_int_offset = 2,
  };

  static Address sh_map_core(Address irq, Cpu_phys_id core)
  {
    return Sh_map_vpe + irq * 0x20UL
           + (cxx::int_value<Cpu_phys_id>(core) / Reg_width) * Reg_bytes;
  }

  static Reg_type sh_map_core_bit(Cpu_phys_id core)
  { return 1UL << (cxx::int_value<Cpu_phys_id>(core) % Reg_width); }

  static Address sh_map_pin(Address irq)
  { return Sh_map_pin + irq * 0x4UL; }

  static Address sh_irq_reg(Address reg, Address irq)
  { return reg + (irq / Reg_width) * Reg_bytes; }

  static Reg_type sh_irq_bit(Address irq)
  { return 1UL << (irq % Reg_width); }

  void mod_mask(Mword pin, bool mask, unsigned hw_reg_base)
  {
    auto b = sh_irq_bit(pin);

    _r[sh_irq_reg(hw_reg_base, pin)] = b;
    if (mask)
      _enabled_map[pin / Reg_width] &= ~b;
    else
      _enabled_map[pin / Reg_width] |= b;
  }

  void setup_ipis();

  Register_block<Reg_width> _r;
  Spin_lock<> _mode_lock;

  Reg_type _enabled_map[256 / Reg_width];
};

