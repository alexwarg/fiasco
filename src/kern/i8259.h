#pragma once

#include <cxx/atomic>

#include "lock_guard.h"
#include "irq_chip.h"
#include "spin_lock.h"
#include "types.h"
#include "pm.h"

#include "globalconfig.h"

template<typename IO>
class Irq_i8259_base
{
public:
  using Io_address = typename IO::Port_addr;

  /**
   * Create a i8259 chip, does not do any hardware access.
   * \note Hardware initalization is done in init().
   */
  constexpr Irq_i8259_base(Io_address master, Io_address slave)
  : master(master), slave(slave)
  {}

  Unsigned16 disable_all_save()
  {
    Unsigned16 s =   static_cast<Unsigned16>(read_ocw_m())
                   | static_cast<Unsigned16>(read_ocw_s()) << 8;
    write_ocw_m(0xff);
    write_ocw_s(0xff);
    return s;
  }

  void restore_all(Unsigned16 s)
  {
    write_ocw_m(s & 0x0ff);
    write_ocw_s((s >> 8) & 0x0ff);
  }

  /**
   * Initialize the i8259 hardware.
   * \pre The IO access must be enabled in the constructor if needed,
   *      for example when using memory-mapped registers.
   */
  void init(Unsigned8 vect_base, bool use_sfn = false,
            bool high_prio_ir8 = false)
  {
    _sfn = use_sfn;
    // disable all IRQs
    write_ocw_m(0xff);
    write_ocw_s(0xff);

    write_icw_m(PICM_ICW1); iodelay();
    write_ocw_m(vect_base); iodelay();
    write_ocw_m((1U << 2)); iodelay(); // cascade at IR2
    Unsigned8 icw4 = PICM_ICW4;
    if (use_sfn)
      icw4 |= SNF_MODE_ENA;
    write_ocw_m(icw4); iodelay();

    write_icw_s(PICS_ICW1); iodelay();
    write_ocw_s(vect_base + 8); iodelay();
    write_ocw_s(PICS_ICW3); iodelay();
    write_ocw_s(icw4); iodelay();

    if (use_sfn && high_prio_ir8)
      {
        // setting specific rotation (specific priority) 
        // -- see Intel 8259A reference manual
        // irq 1 on master hast lowest prio
        // => irq 2 (cascade) = irqs 8..15 have highest prio
        write_icw_m(SET_PRIORITY | 1); iodelay();
        // irq 7 on slave has lowest prio
        // => irq 0 on slave (= irq 8) has highest prio
        write_icw_s(SET_PRIORITY | 7); iodelay();
      }

    // set initial masks
    write_ocw_m(0xfb); iodelay(); // unmask ir2
    write_ocw_s(0xff); iodelay();  // mask everything

    /* Ack any bogus intrs by setting the End Of Interrupt bit. */
    write_icw_m(NON_SPEC_EOI); iodelay();
    write_icw_s(NON_SPEC_EOI); iodelay();
  }

  enum
  {
    OFF_ICW = 0x00,
    OFF_OCW = 0x01,
  };

  enum
  {
    // ICW1
    ICW_TEMPLATE    = 0x10,

    LEVL_TRIGGER    = 0x08,
    EDGE_TRIGGER    = 0x00,
    ADDR_INTRVL4    = 0x04,
    ADDR_INTRVL8    = 0x00,
    SINGLE__MODE    = 0x02,
    CASCADE_MODE    = 0x00,
    ICW4__NEEDED    = 0x01,
    NO_ICW4_NEED    = 0x00,

    // ICW3
    SLAVE_ON_IR0    = 0x01,
    SLAVE_ON_IR1    = 0x02,
    SLAVE_ON_IR2    = 0x04,
    SLAVE_ON_IR3    = 0x08,
    SLAVE_ON_IR4    = 0x10,
    SLAVE_ON_IR5    = 0x20,
    SLAVE_ON_IR6    = 0x40,
    SLAVE_ON_IR7    = 0x80,

    I_AM_SLAVE_0    = 0x00,
    I_AM_SLAVE_1    = 0x01,
    I_AM_SLAVE_2    = 0x02,
    I_AM_SLAVE_3    = 0x03,
    I_AM_SLAVE_4    = 0x04,
    I_AM_SLAVE_5    = 0x05,
    I_AM_SLAVE_6    = 0x06,
    I_AM_SLAVE_7    = 0x07,

    // ICW4
    SNF_MODE_ENA    = 0x10,
    SNF_MODE_DIS    = 0x00,
    BUFFERD_MODE    = 0x08,
    NONBUFD_MODE    = 0x00,
    AUTO_EOI_MOD    = 0x02,
    NRML_EOI_MOD    = 0x00,
    I8086_EMM_MOD   = 0x01,
    SET_MCS_MODE    = 0x00,

    // OCW1
    PICM_MASK       = 0xFF,
    PICS_MASK       = 0xFF,

    // OCW2
    NON_SPEC_EOI    = 0x20,
    SPECIFIC_EOI    = 0x60,
    ROT_NON_SPEC    = 0xa0,
    SET_ROT_AEOI    = 0x80,
    RSET_ROTAEOI    = 0x00,
    ROT_SPEC_EOI    = 0xe0,
    SET_PRIORITY    = 0xc0,
    NO_OPERATION    = 0x40,

    SND_EOI_IR0    = 0x00,
    SND_EOI_IR1    = 0x01,
    SND_EOI_IR2    = 0x02,
    SND_EOI_IR3    = 0x03,
    SND_EOI_IR4    = 0x04,
    SND_EOI_IR5    = 0x05,
    SND_EOI_IR6    = 0x06,
    SND_EOI_IR7    = 0x07,

    // OCW3
    OCW_TEMPLATE    = 0x08,
    SPECIAL_MASK    = 0x40,
    MASK_MDE_SET    = 0x20,
    MASK_MDE_RST    = 0x00,
    POLL_COMMAND    = 0x04,
    NO_POLL_CMND    = 0x00,
    READ_NEXT_RD    = 0x02,
    READ_IR_ONRD    = 0x00,
    READ_IS_ONRD    = 0x01,

    // Standard PIC initialization values for PCs.
    PICM_ICW1       = ICW_TEMPLATE | EDGE_TRIGGER | ADDR_INTRVL8
                      | CASCADE_MODE | ICW4__NEEDED,
    PICM_ICW3       = SLAVE_ON_IR2,
    PICM_ICW4       = SNF_MODE_DIS | NONBUFD_MODE | NRML_EOI_MOD
                      | I8086_EMM_MOD,

    PICS_ICW1       = ICW_TEMPLATE | EDGE_TRIGGER | ADDR_INTRVL8
                      | CASCADE_MODE | ICW4__NEEDED,
    PICS_ICW3       = I_AM_SLAVE_2,
    PICS_ICW4       = SNF_MODE_DIS | NONBUFD_MODE | NRML_EOI_MOD
                      | I8086_EMM_MOD,
  };

  constexpr Io_address ocw_m() const { return master + OFF_OCW; }
  constexpr Io_address icw_m() const { return master + OFF_ICW; }
  constexpr Io_address ocw_s() const { return slave + OFF_OCW; }
  constexpr Io_address icw_s() const { return slave + OFF_ICW; }


  Unsigned8 read_ocw_m()
  { return IO::in8(ocw_m()); }

  void write_ocw_m(Unsigned8 val)
  { IO::out8(val, ocw_m()); }

  Unsigned8 read_icw_m()
  { return IO::in8(icw_m()); }

  void write_icw_m(Unsigned8 val)
  { IO::out8(val, icw_m()); }

  Unsigned8 read_ocw_s()
  { return IO::in8(ocw_s()); }

  void write_ocw_s(Unsigned8 val)
  { IO::out8(val, ocw_s()); }

  Unsigned8 read_icw_s()
  { return IO::in8(icw_s()); }

  void write_icw_s(Unsigned8 val)
  { IO::out8(val, icw_s()); }

  void iodelay() const
  { IO::iodelay(); }

  void mask(Mword pin)
  {
    if (pin < 8)
      write_ocw_m(read_ocw_m() | (1U << pin));
    else
      write_ocw_s(read_ocw_s() | (1U << (pin - 8)));
  }

  void ack(Mword pin)
  {
    if (pin >= 8)
      {
        write_icw_s(NON_SPEC_EOI);
        if (_sfn)
          {
            write_icw_s(OCW_TEMPLATE | READ_NEXT_RD | READ_IS_ONRD);
            if (read_icw_s())
              return; // still active IRQs at the slave, don't EOI master
          }
      }
    write_icw_m(NON_SPEC_EOI);
  }

  Io_address master, slave;
  bool _sfn = false;
};

template<typename IO>
class Irq_chip_i8259 :
  public Irq_chip_icu,
  private Pm_object,
  private Irq_i8259_base<IO>
{
  friend class Jdb_kern_info_pic_state;
  using Base = Irq_i8259_base<IO>;

public:
  IRQ_CHIP_DBG_INFO("i8259");

  typedef typename IO::Port_addr Io_address;
  unsigned nr_irqs() const override { return 16; }
  int set_mode(Mword, Mode) override { return 0; }
  bool is_edge_triggered(Mword) const override { return false; }
  void set_cpu(Mword, Cpu_number) override {}
  using Pm_object::register_pm;

  /**
   * Create a i8259 chip, does not do any hardware access.
   * \note Hardware initalization is done in init().
   */
  Irq_chip_i8259(Irq_chip_i8259::Io_address master,
                 Irq_chip_i8259::Io_address slave)
  : Base(master, slave)
  {}

  /**
   * Initialize the i8259 hardware.
   * \pre The IO access must be enabled in the constructor if needed,
   *      for example when using memory-mapped registers.
   */
  void init(Unsigned8 vect_base, bool use_sfn = false,
            bool high_prio_ir8 = false)
  {
    auto g = lock_guard(_lock);
    Base::init(vect_base, use_sfn, high_prio_ir8);
  }

  void mask(Mword pin) override
  {
    auto g = lock_guard(_lock);
    Base::mask(pin);
  }

  void unmask(Mword pin) override
  {
    auto g = lock_guard(_lock);
    if (pin < 8)
      this->write_ocw_m(this->read_ocw_m() & ~(1U << pin));
    else
      this->write_ocw_s(this->read_ocw_s() & ~(1U << (pin - 8)));
  }

  void ack(Mword pin) override
  {
    auto g = lock_guard(_lock);
    Base::ack(pin);
  }

  void mask_and_ack(Mword pin) override
  {
    auto g = lock_guard(_lock);
    Base::mask(pin);
    Base::ack(pin);
  }

private:
  // power-management hooks
  void pm_on_suspend(Cpu_number) override
  { _pm_saved_state = this->disable_all_save(); }

  void pm_on_resume(Cpu_number) override
  { this->restore_all(_pm_saved_state); }

  Spin_lock<> _lock;

  // power-management state
  Unsigned16 _pm_saved_state;
};

template<typename IO>
class Irq_chip_i8259_gen : public Irq_chip_i8259<IO>
{
public:
  typedef typename Irq_chip_i8259<IO>::Io_address Io_address;
  Irq_chip_i8259_gen(Io_address master, Io_address slave)
  : Irq_chip_i8259<IO>(master, slave)
  {
    for (auto &i: _irqs)
      i = 0;
  }

  Irq_base *irq(Mword pin) const override
  {
    if (pin >= 16)
      return 0;

    return _irqs[pin];
  }

  bool alloc(Irq_base *irq, Mword pin, bool init = true) override
  {
    if (pin >= 16)
      return false;

    if (_irqs[pin])
      return false;

    Irq_base *none = nullptr;
    if (!cxx::atomic_compare_exchange_strong(&_irqs[pin], none, irq))
      return false;

    this->bind(irq, pin, !init);
    return true;
  }

  bool reserve(Mword pin) override
  {
    if (pin >= 16)
      return false;

    if (_irqs[pin])
      return false;

    _irqs[pin] = reinterpret_cast<Irq_base*>(1);

    return true;
  }

  void unbind(Irq_base *irq) override
  {
    _irqs[irq->pin()] = 0;
    Irq_chip_icu::unbind(irq);
  }

private:
  Irq_base *_irqs[16];

};

