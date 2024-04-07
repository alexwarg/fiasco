#pragma once

#include "acpi.h"
#include "div32.h"
#include "mem.h"

#include "globalconfig.h"

class Acpi_hpet : public Acpi_table_head
{
public:
  Unsigned32 event_timer_block_id;
  Acpi_gas   base_address;
  Unsigned8  hpet_number;
  Unsigned16 min_clock_ticks_for_irq;
  Unsigned8  page_prot_and_oem_attr;
} __attribute__((packed));

class Hpet_timer
{
public:
  Unsigned64 config_and_cap;
  Unsigned64 comp_val;
  Unsigned64 irq_route;
  Unsigned64 _pad0;

  enum
  {
    Tn_INT_TYPE_CNF       = 1 << 1,
    Tn_INT_ENB_CNF        = 1 << 2,
    Tn_TYPE_CNF           = 1 << 3,
    Tn_PER_INT_CAP        = 1 << 4,
    Tn_SIZE_CAP           = 1 << 5,
    Tn_VAL_SET_CNF        = 1 << 6,
    Tn_32MODE_CNF         = 1 << 8,
    Tn_FSB_EN_CNF         = 1 << 14,
    Tn_FSB_INT_DEL_CAP    = 1 << 15,
    Tn_INT_ROUTE_CNF_MASK = 31 << 9,
  };

  void enable_int()  { config_and_cap |=  Tn_INT_ENB_CNF; }
  void disable_int() { config_and_cap &= ~Tn_INT_ENB_CNF; }
  void set_int(int irq)
  { config_and_cap = (config_and_cap & ~Tn_INT_ROUTE_CNF_MASK) | (irq << 9); }

  int get_int() const { return (config_and_cap & Tn_INT_ROUTE_CNF_MASK) >> 9; }

  bool can_periodic() const { return config_and_cap & Tn_PER_INT_CAP; }
  void set_periodic()       { config_and_cap |= Tn_TYPE_CNF; }

  void set_level_irq() { config_and_cap |=  Tn_INT_TYPE_CNF; }
  void set_edge_irq()  { config_and_cap &= ~Tn_INT_TYPE_CNF; }

  bool can_64bit() const { return config_and_cap & Tn_SIZE_CAP; }
  void force_32bit()     { config_and_cap |= Tn_32MODE_CNF; }
  void val_set()         { config_and_cap |= Tn_VAL_SET_CNF; }

  Unsigned32 int_route_cap() const { return config_and_cap >> 32; }
  bool       int_avail(int i) const { return int_route_cap() & i; }

  int get_first_int()
  {
    Unsigned32 cap = int_route_cap();
    for (int i = 0; i < 32; ++i)
      if (cap & (1 << i))
        return i;
    return ~0U;
  }
} __attribute__((packed));

class Hpet_device
{
private:
public:
  Unsigned64 cap_and_id;        // offset 0x00
  Unsigned64 _pad0;
  Unsigned64 config;            // offset 0x10
  Unsigned64 _pad1;
  Unsigned64 irq_status;        // offset 0x20
  Unsigned64 _pad2[1 + 2 * 12];
  Unsigned64 counter_val;       // 0ffset 0xf0

  enum Config
  {
    ENABLE_CNF = 1 << 0,
    LEG_RT_CNF = 1 << 1,
  };

public:

  int num_timers() const { return ((cap_and_id >> 8) & 0xf) + 1; }

  Hpet_timer *timer(int idx) const
  { return reinterpret_cast<Hpet_timer *>((char *)this + 0x100 + idx * 0x20); }

  void enable()  { config |= ENABLE_CNF; Mem::mb(); }
  void disable() { config &= ~ENABLE_CNF; Mem::mb(); }

  Unsigned32 counter_clk_period() const { return cap_and_id >> 32; }
  void reset_counter() { counter_val = 0; Mem::mb(); }

  void clear_level_irq(int timer_nr)
  { irq_status = 1 << timer_nr; Mem::mb(); }


  void dump() const;

} __attribute__((packed));

class Hpet
{
public:
  static void set_periodic()   { _hpet_timer->set_periodic(); }
  static void enable_timer()   { _hpet_timer->enable_int(); }
  static void disable_timer()  { _hpet_timer->disable_int(); }
  static void clear_timer()    { /* _hpet->clear_level_irq(2); */ }
  static int  int_num()        { return _hpet_timer->get_int(); }
  static bool int_avail(int i) { return _hpet_timer->int_avail(i); }

  static void enable()         { _hpet->enable(); }
  static void dump() { _hpet->dump(); }

  static void dump_acpi_infos();

  static bool init();

#if defined (CONFIG_JDB)
  static Hpet_device *hpet()
  { return _hpet; }
#endif // CONFIG_JDB

private:
  static Acpi_hpet const *_acpi_hpet;
  static Hpet_device *_hpet;
  static Hpet_timer *_hpet_timer;
};

