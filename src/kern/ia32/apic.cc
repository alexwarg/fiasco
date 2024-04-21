#include <apic.h>

#include <kmem.h>
#include <kip.h>
#include <initcalls.h>
#include <entry_frame.h>
#include <pit_i8254.h>

#include <warn.h>
#include <cstdio>

DEFINE_PER_CPU Per_cpu<Static_object<Apic> >  Apic::apic;

unsigned apic_spurious_interrupt_bug_cnt;
unsigned apic_spurious_interrupt_cnt;
unsigned apic_error_cnt;
Address  apic_io_base;

unsigned   Apic::present;
int        Apic::good_cpu;
const Address Apic::io_base = Mem_layout::Local_apic_page;
Address    Apic::phys_base;
unsigned   Apic::timer_divisor = 1;
unsigned   Apic::frequency_khz;
Unsigned64 Apic::scaler_us_to_apic;

int ignore_invalid_apic_reg_access;

enum
{
  APIC_tdr_div_1		= 0xB,
  APIC_tdr_div_2		= 0x0,
  APIC_tdr_div_4		= 0x1,
  APIC_tdr_div_8		= 0x2,
  APIC_tdr_div_16		= 0x3,
  APIC_tdr_div_32		= 0x8,
  APIC_tdr_div_64		= 0x9,
  APIC_tdr_div_128		= 0xA,
};

void
Apic::timer_set_divisor(unsigned newdiv)
{
  int i;
  int div = -1;
  int divval = newdiv;
  Unsigned32 tmp_value;

  static int divisor_tab[8] =
    {
      APIC_tdr_div_1,  APIC_tdr_div_2,  APIC_tdr_div_4,  APIC_tdr_div_8,
      APIC_tdr_div_16, APIC_tdr_div_32, APIC_tdr_div_64, APIC_tdr_div_128
    };

  for (i=0; i<8; i++)
    {
      if (divval & 1)
	{
	  if (divval & ~1)
	    {
	      printf("bad APIC divisor %u\n", newdiv);
	      return;
	    }
	  div = divisor_tab[i];
	  break;
	}
      divval >>= 1;
    }

  if (div != -1)
    {
      Apic::timer_divisor = newdiv;
      tmp_value = Apic::reg_read(Apic::APIC_tdcr);
      tmp_value &= ~0x1F;
      tmp_value |= div;
      Apic::reg_write(Apic::APIC_tdcr, tmp_value);
    }
}

// set the global pagetable entry for the Local APIC device registers
FIASCO_INIT_AND_PM
void
Apic::map_apic_page()
{
  Address offs;
  Address base = apic_page_phys();
  // We should not change the physical address of the Local APIC page if
  // possible since some versions of VMware would complain about a
  // non-implemented feature
  Kmem::map_phys_page(base, Mem_layout::Local_apic_page,
		      false, true, &offs);

  Kip::k()->add_mem_region(Mem_desc(base, base + Config::PAGE_SIZE - 1, Mem_desc::Reserved));

  assert(offs == 0);
}

// check CPU type if APIC could be present
FIASCO_INIT_AND_PM
int
Apic::test_cpu(Cpu *cpu)
{
  if (!cpu->can_wrmsr() || !(cpu->features() & FEAT_TSC))
    return 0;

  if (cpu->vendor() == Cpu::Vendor_intel)
    {
      if (cpu->family() == 15)
	return 1;
      if (cpu->family() >= 6)
	return 1;
    }
  if (cpu->vendor() == Cpu::Vendor_amd && cpu->family() >= 6)
    return 1;

  return 0;
}

FIASCO_INIT_AND_PM
int
Apic::check_working()
{
  Unsigned64 tsc_until;

  timer_disable_irq();
  timer_set_divisor(1);
  timer_reg_write(0x10000000);

  tsc_until = Cpu::rdtsc() + 0x400;  // we only have to wait for one bus cycle

  do
    {
      if (timer_reg_read() != 0x10000000)
        return 1;
    } while (Cpu::rdtsc() < tsc_until);

  return 0;
}

FIASCO_INIT_CPU_AND_PM
void
Apic::init_spiv()
{
  Unsigned32 tmp_val;

  tmp_val = reg_read(APIC_spiv);
  tmp_val |= (1<<8);            // enable APIC
  tmp_val &= ~(1<<9);           // enable Focus Processor Checking
  tmp_val &= ~0xff;
  tmp_val |= APIC_IRQ_BASE + 0xf; // Set spurious IRQ vector to 0x3f
                              // bit 0..3 are hardwired to 1 on PPro!
  reg_write(APIC_spiv, tmp_val);
}

FIASCO_INIT_CPU_AND_PM
void
Apic::enable_errors()
{
  if (is_integrated())
    {
      Unsigned32 tmp_val, before, after;

      if (get_max_lvt() > 3)
	clear_num_errors();
      before = get_num_errors();

      tmp_val = reg_read(APIC_lvterr);
      tmp_val &= 0xfffeff00;         // unmask error IRQ vector
      tmp_val |= APIC_IRQ_BASE + 3;  // Set error IRQ vector to 0x63
      reg_write(APIC_lvterr, tmp_val);

      if (get_max_lvt() > 3)
	clear_num_errors();
      after = get_num_errors();
      if (Warn::is_enabled(Info))
        printf("APIC ESR value before/after enabling: %08x/%08x\n",
               before, after);
    }
}

FIASCO_INIT_AND_PM
void
Apic::route_pic_through_apic()
{
  Unsigned32 tmp_val;
  auto guard = lock_guard(cpu_lock);

  // mask 8259 interrupts
  Unsigned16 old_irqs = Pic::disable_all_save();

  // set LINT0 to ExtINT, edge triggered
  tmp_val = reg_read(APIC_lvt0);
  tmp_val &= 0xfffe5800;
  tmp_val |= 0x00000700;
  reg_write(APIC_lvt0, tmp_val);

  // set LINT1 to NMI, edge triggered
  tmp_val = reg_read(APIC_lvt1);
  tmp_val &= 0xfffe5800;
  tmp_val |= 0x00000400;
  reg_write(APIC_lvt1, tmp_val);

  // unmask 8259 interrupts
  Pic::restore_all(old_irqs);

  printf("APIC was disabled --- routing PIC through APIC\n");
}

FIASCO_INIT_CPU_AND_PM
void
Apic::init_lvt()
{
  auto guard = lock_guard(cpu_lock);

  // mask timer interrupt and set vector to _not_ invalid value
  reg_write(APIC_lvtt, reg_read(APIC_lvtt) | APIC_lvt_masked | 0xff);

  if (have_pcint())
    // mask performance interrupt and set vector to a valid value
    reg_write(APIC_lvtpc, reg_read(APIC_lvtpc) | APIC_lvt_masked | 0xff);

  if (have_tsint())
    // mask thermal sensor interrupt and set vector to a valid value
    reg_write(APIC_lvtthmr, reg_read(APIC_lvtthmr) | APIC_lvt_masked | 0xff);
}

int
Apic::test_present_but_disabled()
{
  if (!good_cpu)
    return 0;

  Unsigned64 msr = Cpu::rdmsr(APIC_base_msr);
  return ((msr & 0xffffff000ULL) == 0xfee00000ULL);
}

FIASCO_INIT_CPU_AND_PM
void
Apic::activate_by_msr()
{
  Unsigned64 msr;

  msr = Cpu::rdmsr(APIC_base_msr);
  phys_base = msr & 0xfffff000;
  msr |= (1<<11);
  Cpu::wrmsr(msr, APIC_base_msr);

  // later we have to call update_feature_info() as the flags may have changed
}

FIASCO_INIT_CPU
int
Apic::check_still_getting_interrupts()
{
  if (!Config::apic)
    return 0;

  Unsigned64 tsc_until;
  Cpu_time clock_start = Kip::k()->clock();

  tsc_until = Cpu::rdtsc();
  tsc_until += 0x01000000; // > 10 Mio cycles should be sufficient until
                           // we have processors with more than 10 GHz
  do
    {
      // kernel clock by timer interrupt updated?
      if (Kip::k()->clock() != clock_start)
	// yes, successful
	return 1;
    } while (Cpu::rdtsc() < tsc_until);

  // timeout
  return 0;
}

FIASCO_INIT_CPU_AND_PM
void
Apic::calibrate_timer(Cpu *cpu)
{
  const unsigned calibrate_time = 50;
  Unsigned32 count, tt1, tt2, result, dummy;
  Unsigned32 runs = 0, frequency_ok;

  do
    {
      frequency_khz = 0;

      timer_disable_irq();
      timer_set_divisor(1);
      timer_reg_write(1000000000);

      if (cpu->tsc_frequency_accurate())
        {
          auto guard = lock_guard(cpu_lock);
          tt1 = timer_reg_read();
          cpu->busy_wait_ns(50000000ULL);  // 20Hz
          tt2 = timer_reg_read();
          count = cpu->ns_to_tsc(50000000ULL);
        }
      else
        {
          auto guard = lock_guard(cpu_lock);

          Pit::setup_channel2_to_20hz();

          count = 0;

          tt1 = timer_reg_read();
          do
            {
              count++;
            }
          while ((Io::in8(0x61) & 0x20) == 0);
          tt2 = timer_reg_read();
        }

      result = (tt1 - tt2) * timer_divisor;

      // APIC not running
      if (count <= 1)
        return;

      asm ("divl %2"
          :"=a" (frequency_khz), "=d" (dummy)
          : "r" (calibrate_time), "a" (result), "d" (0));

      frequency_ok = (frequency_khz < (1000<<11));
    }
  while (++runs < 10 && !frequency_ok);

  if (!frequency_ok)
    panic("APIC frequency too high, adapt Apic::scaler_us_to_apic");

  Kip::k()->frequency_bus = frequency_khz;
  scaler_us_to_apic       = Cpu::muldiv(1<<21, frequency_khz, 1000);
}

void
Apic::error_interrupt(Return_frame *regs)
{
  Unsigned32 err1, err2;

  // we are entering with disabled interrupts
  err1 = Apic::get_num_errors();
  Apic::clear_num_errors();
  err2 = Apic::get_num_errors();
  Apic::irq_ack();

  cpu_lock.clear();

  if (err1 == 0x80 || err2 == 0x80)
    {
      // ignore possible invalid access which may happen in
      // jdb::do_dump_memory()
      if (ignore_invalid_apic_reg_access)
	return;

      printf("CPU%u: APIC invalid register access error at " L4_PTR_FMT "\n",
	     cxx::int_value<Cpu_number>(current_cpu()), regs->ip());
      return;
    }

  apic_error_cnt++;
  printf("CPU%u: APIC error %08x(%08x)\n",
         cxx::int_value<Cpu_number>(current_cpu()), err1, err2);
}

void
Apic::done()
{
  Unsigned64 val;

  if (!is_present())
    return;

  val = reg_read(APIC_spiv);
  val &= ~(1<<8);
  reg_write(APIC_spiv, val);

  val = Cpu::rdmsr(APIC_base_msr);
  val  &= ~(1<<11);
  Cpu::wrmsr(val, APIC_base_msr);
}

FIASCO_INIT_CPU_AND_PM
void
Apic::init_timer(Cpu *cpu)
{
  calibrate_timer(cpu);
  timer_set_divisor(1);
  enable_errors();
}

void
Apic::dump_info()
{
  if (Warn::is_enabled(Info))
    printf("Local APIC[%02x]: version=%02x max_lvt=%d\n",
           get_id() >> 24, get_version(), get_max_lvt());
}

FIASCO_INIT_CPU_AND_PM
void
Apic::map_registers()
{
  Cpu *cpu = Cpu::boot_cpu();

  if (test_present(cpu))
    {
      set_present();
      set_present_before_msr();
    }
  else
    {
      clear_present();
      clear_present_before_msr();
    }

  if (!is_present_before_msr())
    {
      good_cpu = test_cpu(cpu);

      if (good_cpu && Config::apic)
        {
          // activate; this could lead an disabled APIC to appear
          // set base address of I/O registers to be able to access the registers
          activate_by_msr();
          cpu->update_features_info();
          if (test_present(cpu))
            set_present();
        }
    }

  if (!Config::apic)
    return;

  // initialize if available
  if (is_present())
    // map the Local APIC device registers
    map_apic_page();
}

FIASCO_INIT_CPU_AND_PM
void
Apic::init(bool resume)
{
  Cpu *cpu = Cpu::boot_cpu();

  // FIXME: reset cached CPU features, we should add a special function
  // for the apic bit
  if (resume)
    cpu->update_features_info();

  if (!Config::apic)
    return;

  // initialize if available
  if (is_present())
    {
      // set some interrupt vectors to appropriate values
      init_lvt();

      // initialize APIC_spiv register
      init_spiv();

      // initialize task-priority register
      init_tpr();

      // test if local timer counts down
      if (check_working())
        {
          if (!is_present_before_msr())
            // APIC _was_ not present before writing to msr so we have
            // to set APIC_lvt0 and APIC_lvt1 to appropriate values
            route_pic_through_apic();
        }
      else
        clear_present();
    }

  if (!is_present())
    panic("Local APIC not found");

  dump_info();

  apic_io_base = Mem_layout::Local_apic_page;
  init_timer(cpu);
}
