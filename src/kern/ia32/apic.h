#pragma once

#include <types.h>
#include <pm.h>
#include <cxx/cxx_int>
#include <per_cpu_data.h>
#include <processor.h>
#include <cpu.h>
#include <apic_id.h>
#include <alternatives.h>

#include <globalconfig.h>
#include <cassert>

class Return_frame;

class Apic : public Pm_object
{
public:
  enum
  {
    APIC_id			= 0x20,
    APIC_lvr			= 0x30,
    APIC_tpr                    = 0x80,
    APIC_tpri_mask		= 0xFF,
    APIC_eoi			= 0xB0,
    APIC_ldr			= 0xD0,
    APIC_ldr_mask		= 0xFFul << 24,
    APIC_dfr			= 0xE0,
    APIC_spiv			= 0xF0,
    APIC_isr			= 0x100,
    APIC_tmr			= 0x180,
    APIC_irr			= 0x200,
    APIC_esr			= 0x280,
    APIC_lvtt			= 0x320,
    APIC_lvtthmr		= 0x330,
    APIC_lvtpc			= 0x340,
    APIC_lvt0			= 0x350,
    APIC_timer_base_div		= 0x2,
    APIC_lvt1			= 0x360,
    APIC_lvterr			= 0x370,
    APIC_tmict			= 0x380,
    APIC_tmcct			= 0x390,
    APIC_tdcr			= 0x3E0,
  };

  enum
  {
    APIC_snd_pending		= 1 << 12,
    APIC_input_polarity		= 1 << 13,
    APIC_lvt_remote_irr		= 1 << 14,
    APIC_lvt_level_trigger	= 1 << 15,
    APIC_lvt_masked		= 1 << 16,
    APIC_lvt_timer_periodic	= 1 << 17,
  };

  enum class Ipi_dest_shrt : Unsigned8
  {
    Noshrt = 0b00,
    Self   = 0b01,
    All    = 0b10,
    Others = 0b11,
  };

  enum class Ipi_delivery_mode : Unsigned8
  {
    Fixed   = 0b000,
    Nmi     = 0b100,
    Init    = 0b101,
    Startup = 0b110,
  };

  enum
  {
    Mask			=  1,
    Trigger_mode		=  2,
    Remote_irr			=  4,
    Pin_polarity		=  8,
    Delivery_state		= 16,
    Delivery_mode		= 32,
  };

  static void map_registers();
  static void init(bool resume = false);
  static void map_apic_page();
  static int test_cpu(Cpu *cpu);
  static int test_present_but_disabled();
  static void activate_by_msr();
  static void done();
  static void dump_info();

  struct use_x2apic : public Alternative_static_functor<use_x2apic>
  {
    static bool probe() { return Apic::use_x2; }
  };

  Apic_id apic_id() const { return _id; }
  Cpu_phys_id cpu_id() const
  {
    Unsigned32 r = cxx::int_value<Apic_id>(_id);
    return Cpu_phys_id{r};
  }

  static Per_cpu<Static_object<Apic> > apic;

  static constexpr Unsigned32 Timer_max = UINT32_MAX;

  Apic(Cpu_number cpu) : _id(get_id()) { register_pm(cpu); }

  static Cpu_number find_cpu(Apic_id phys_id)
  {
    return apic.find_cpu([phys_id](Apic const *a)
                         { return a && a->apic_id() == phys_id; });
  }

  void pm_on_suspend(Cpu_number) override
  {
    _saved_apic_timer = timer_reg_read();
  }

  void pm_on_resume(Cpu_number cpu) override
  {
    if (!IS_ENABLED(CONFIG_MP) || (cpu == Cpu_number::boot_cpu()))
      Apic::init(true);
    else
      Apic::init_ap();
    timer_reg_write(_saved_apic_timer);
  }

  static Unsigned32 get_version()
  {
    return reg_read(APIC_lvr) & 0xFF;
  }

  /**
   * APIC identifier of the current CPU.
   */
  static Apic_id get_id()
  {
    if (use_x2apic())
      return Apic_id{reg_read_x2(APIC_id)};
    else
      return Apic_id{reg_read_x(APIC_id) & 0xff000000};
  }

  /**
   * APIC identifier generated from 1-byte MADT Local APIC structure APIC ID.
   */
  static Apic_id acpi_lapic_to_apic_id(Unsigned32 acpi_id)
  {
    if (use_x2apic())
      return Apic_id{acpi_id};
    else
      return Apic_id{Unsigned32{acpi_id} << 24};
  }

  static void irq_ack()
  {
    reg_read(APIC_spiv);
    reg_write(APIC_eoi, 0);
  }

  static void tpr(unsigned prio)
  { reg_write(APIC_tpr, prio); }

  static unsigned tpr()
  { return reg_read(APIC_tpr); }

  static void init_tpr()
  { reg_write(APIC_tpr, 0); }

  static unsigned get_frequency_khz()
  {
    return frequency_khz;
  }

  static Unsigned32 reg_read_x(unsigned reg)
  {
    return *offset_cast<volatile Unsigned32 *>(io_base, reg);
  }

  static Unsigned32 reg_read_x2(unsigned reg)
  {
    return Cpu::rdmsr(APIC_msr_base + (reg >> 4));
  }

  static Unsigned32 reg_read(unsigned reg)
  {
    if (use_x2apic())
      return reg_read_x2(reg);
    else
      return reg_read_x(reg);
  }

  static void reg_write_x(unsigned reg, Unsigned32 val)
  {
    *offset_cast<volatile Unsigned32 *>(io_base, reg) = val;
  }

  static void reg_write_x2(unsigned reg, Unsigned32 val)
  {
    Cpu::wrmsr(val, 0, APIC_msr_base + (reg >> 4));
  }

  static void reg_write(unsigned reg, Unsigned32 val)
  {
    if (use_x2apic())
      reg_write_x2(reg, val);
    else
      reg_write_x(reg, val);
  }

  static void reg_write64(unsigned reg, Unsigned64 val)
  {
    assert(use_x2);
    Cpu::wrmsr(val, APIC_msr_base + (reg >> 4));
  }

  static int reg_delivery_mode(Unsigned32 val)
  {
    return (val >> 8) & 7;
  }

  static int reg_lvt_vector(Unsigned32 val)
  {
    return val & 0xff;
  }

  static Unsigned32 timer_reg_read()
  {
    return reg_read(APIC_tmcct);
  }

  static Unsigned32 timer_reg_read_initial()
  {
    return reg_read(APIC_tmict);
  }

  static void timer_reg_write(Unsigned32 val)
  {
    reg_read(APIC_tmict);
    reg_write(APIC_tmict, val);
  }

  static void set_perf_nmi()
  {
    if (have_pcint())
      reg_write(APIC_lvtpc, 0x400);
  }

  static Address apic_page_phys()
  { return Cpu::rdmsr(APIC_base_msr) & 0xfffff000; }


  static bool have_pcint()
  {
    return (is_present() && (get_max_lvt() >= 4));
  }

  static bool have_tsint()
  {
    return (is_present() && (get_max_lvt() >= 5));
  }


  static Unsigned32 us_to_apic(Unsigned64 us)
  {
#ifdef CONFIG_BIT32
    Unsigned32 apic, dummy1, dummy2;
    asm ("movl  %%edx, %%ecx		\n\t"
         "mull  %4			\n\t"
         "movl  %%ecx, %%eax		\n\t"
         "movl  %%edx, %%ecx		\n\t"
         "mull  %4			\n\t"
         "addl  %%ecx, %%eax		\n\t"
         "shll  $11, %%eax		\n\t"
        :"=a" (apic), "=d" (dummy1), "=&c" (dummy2)
        : "A" (us),   "g" ((Unsigned32)scaler_us_to_apic)
          // scaler_us_to_apic is actually 32-bit
         );
    return apic;
#else
    Unsigned32 apic, dummy;
    asm ("mulq  %3			\n\t"
         "shrq  $21,%%rax			\n\t"
        :"=a"(apic), "=d"(dummy)
        :"a"(us), "g"(scaler_us_to_apic)
        );
    return apic;
#endif
}


  static int is_present()
  {
    return ((present & Present) == Present);
  }

  static void timer_enable_irq()
  {
    Unsigned32 tmp_val;

    tmp_val = reg_read(APIC_lvtt);
    tmp_val &= ~(APIC_lvt_masked);
    reg_write(APIC_lvtt, tmp_val);
  }

  static void timer_disable_irq()
  {
    Unsigned32 tmp_val;

    tmp_val = reg_read(APIC_lvtt);
    tmp_val |= APIC_lvt_masked;
    reg_write(APIC_lvtt, tmp_val);
  }

  static bool timer_is_irq_enabled()
  {
    return ~reg_read(APIC_lvtt) & APIC_lvt_masked;
  }

  static void timer_set_periodic()
  {
    Unsigned32 tmp_val = reg_read(APIC_lvtt);
    tmp_val |= APIC_lvt_timer_periodic;
    reg_write(APIC_lvtt, tmp_val);
  }

  static void timer_set_one_shot()
  {
    Unsigned32 tmp_val = reg_read(APIC_lvtt);
    tmp_val &= ~APIC_lvt_timer_periodic;
    reg_write(APIC_lvtt, tmp_val);
  }

  static void timer_assign_irq_vector(unsigned vector)
  {
    Unsigned32 tmp_val = reg_read(APIC_lvtt);
    tmp_val &= 0xffffff00;
    tmp_val |= vector;
    reg_write(APIC_lvtt, tmp_val);
  }


private:
  Apic(const Apic&) = delete;
  Apic &operator = (Apic const &) = delete;

  Apic_id _id;
  Unsigned32 _saved_apic_timer;

  static void error_interrupt(Return_frame *regs)
    asm ("apic_error_interrupt") FIASCO_FASTCALL;

  static unsigned present;
  static int good_cpu;
  static const Address io_base;
  static Address phys_base;
  static unsigned timer_divisor;
  static unsigned frequency_khz;
  static Unsigned64 scaler_us_to_apic;
  static bool use_x2;


  enum
  {
    APIC_msr_base               = 0x800,
    APIC_base_msr               = 0x1b,
  };

  enum
  {
    Present = 0x01,
    Present_before_msr = 0x02,
  };

  enum
  {
    Apic_icr = 0x300,
    Apic_icr2 = 0x310,
  };

  static bool is_integrated()
  {
    return reg_read(APIC_lvr) & 0xF0;
  }

  static Unsigned32 get_max_lvt_local()
  {
    return ((reg_read(APIC_lvr) >> 16) & 0xFF);
  }

  static Unsigned32 get_num_errors()
  {
    reg_write(APIC_esr, 0);
    return reg_read(APIC_esr);
  }

  static void clear_num_errors()
  {
    reg_write(APIC_esr, 0);
    reg_write(APIC_esr, 0);
  }

  static unsigned get_max_lvt()
  {
    return is_integrated() ? get_max_lvt_local() : 2;
  }

public:
  static void disable_external_ints()
  {
    reg_write(APIC_lvt0, 0x0001003f);
    reg_write(APIC_lvt1, 0x0001003f);
  }

  static bool mp_ipi_idle()
  {
    if (use_x2apic())
      return true;
    else
      return ((reg_read_x(Apic_icr) & 0x00001000) == 0);
  }

  static bool mp_ipi_idle_timeout(Cpu const *c, Unsigned32 wait)
  {
    Unsigned64 wait_till = c->time_us() + wait;
    while (!mp_ipi_idle() && c->time_us() < wait_till)
      Proc::pause();
    return mp_ipi_idle();
  }

  static void mp_send_ipi(Ipi_dest_shrt dest_shrt, Apic_id dest,
                          Ipi_delivery_mode delivery_mode, Unsigned8 vector)
  {
    Unsigned32 const lower_icr =   static_cast<Unsigned32>(dest_shrt) << 18
                                 | static_cast<Unsigned32>(delivery_mode) << 8
                                 | vector;

    while (!mp_ipi_idle())
      Proc::pause();

    if (use_x2apic())
      {
        asm volatile ("mfence; lfence"); // enforce serializing as in xAPIC mode
        Cpu::wrmsr(lower_icr, cxx::int_value<Apic_id>(dest),
                   APIC_msr_base + (Apic_icr >> 4));
      }
    else
      {
        if (dest_shrt == Ipi_dest_shrt::Noshrt)
          reg_write_x(Apic_icr2, cxx::int_value<Apic_id>(dest));
        reg_write_x(Apic_icr, lower_icr);
      }
  }

  static void mp_ipi_ack()
  {
    reg_write(APIC_eoi, 0);
  }


  static int mp_startup(Cpu const *current_cpu, Apic_id dest, bool bcast, Address tramp_page);
  static void init_ap();

private:
  static void timer_set_divisor(unsigned newdiv);
  static int check_working();
  static void init_spiv();
  static void enable_errors();
  static void route_pic_through_apic();
  static void init_lvt();
  static void calibrate_timer(Cpu *cpu);
  static void init_timer(Cpu *cpu);

  static void delay(Cpu const *c, Unsigned32 wait)
  {
    Unsigned64 wait_till = c->time_us() + wait;
    while (c->time_us() < wait_till)
      Proc::pause();
  }

};

extern unsigned apic_spurious_interrupt_bug_cnt;
extern unsigned apic_spurious_interrupt_cnt;
extern unsigned apic_error_cnt;

