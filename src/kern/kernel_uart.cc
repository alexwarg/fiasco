
#include "kernel_uart.h"


#include "uart.h"
#include "std_macros.h"
#include "pm.h"

#include "filter_console.h"
#include "irq_chip.h"
#include "irq_mgr.h"
#include "kdb_ke.h"
#include "kernel_console.h"
#include "uart.h"
#include <uart_base.h>
#include "config.h"
#include "kip.h"
#include "koptions.h"
#include "panic.h"
#include "vkey.h"

#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#include "kmem_mmio.h"
#include "io_regblock.h"
#include "types.h"

#if defined (CONFIG_AMD64) || defined (CONFIG_IA32)
#define HAVE_PORTIO 1
#include "io_regblock_port.h"
#endif

namespace {

/**
 * Glue between kernel and UART driver.
 */
class Kuart : public Uart, public Pm_object
{
private:
  unsigned long _mem[10];
  L4::Uart_iface *_uart;
  /**
   * Prototype for the UART specific startup implementation.
   * @param uart, the instantiation to start.
   * @param port, the com port number.
   */
  bool startup(unsigned port, int irq, bool resume);
  bool setup_uart_io_port(void *r, Address base, int irq, bool resume);

  void setup(bool resume)
  {
    unsigned           n = Config::default_console_uart_baudrate;
    ::Uart::TransferMode m = ::Uart::MODE_8N1;
    unsigned long long p = Config::default_console_uart;
    int                i = -1;

    if (Koptions::o()->opt(Koptions::F_uart_baud))
      n = Koptions::o()->uart.baud;

    if (Koptions::o()->opt(Koptions::F_uart_base))
      p = Koptions::o()->uart.base_address;

    if (Koptions::o()->opt(Koptions::F_uart_irq))
      i = Koptions::o()->uart.irqno;

    if (!startup(p, i, resume))
      printf("Comport/base 0x%04llx is not accepted by the uart driver!\n", p);
    else if (!_uart->change_mode(m, n))
      panic("Something is wrong with the baud rate (%u)!\n", n);
  }

  bool startup(L4::Io_register_block const *reg, int irq, Unsigned32 base_baud,
               bool /*resume*/)
  {
    _irq = irq;
    if (Koptions::o()->opt(Koptions::F_uart_cid))
      _uart = L4::Uart::create_from_cid(Koptions::o()->uart_cid, _mem, sizeof(_mem));
    else
#ifdef FIASCO_UART_TYPE
      _uart = new (_mem) FIASCO_UART_TYPE();
#else
      return false;
#endif

    _uart->set_base_rate(base_baud);

    if (!_uart->startup(reg))
      return false;

    add_state(ENABLED);
    return true;
  }

public:
  Kuart()
  {
    setup(false);
  }

  bool enable_rx_irq(bool val = true) override
  {
    return _uart->enable_rx_irq(val);
  }

  void irq_ack() override
  {
    _uart->irq_ack();
  }

  int write(char const *d, size_t len) override
  {
    return _uart->write(d, len);
  }

  int getchar(bool blocking=true) override
  {
    return _uart->get_char(blocking);
  }

  int char_avail() const override
  {
    return _uart->char_avail();
  }

  void pm_on_suspend(Cpu_number cpu) override
  {
    (void)cpu;
    assert (cpu == Cpu_number::boot_cpu());

    Kernel_uart::uart()->state(Console::DISABLED);

    if(Config::serial_esc != Config::SERIAL_ESC_NOIRQ)
      Kernel_uart::uart()->enable_rx_irq(false);
  }

  void pm_on_resume(Cpu_number cpu) override
  {
    (void)cpu;
    assert (cpu == Cpu_number::boot_cpu());
    static_cast<Kuart*>(Kernel_uart::uart())->setup(true);
    Kernel_uart::uart()->state(Console::ENABLED);

    if(Config::serial_esc != Config::SERIAL_ESC_NOIRQ)
      Kernel_uart::uart()->enable_rx_irq(true);
  }
};

static Static_object<Filter_console> _fcon;
static Static_object<Kuart> _kernel_uart;



class Kuart_irq : public Irq_base
{
public:
  Kuart_irq() { hit_func = &handler_wrapper<Kuart_irq>; }
  void switch_mode(bool) override {}
  void handle(Upstream_irq const *ui)
  {
    Kernel_uart::uart()->irq_ack();
    mask_and_ack();
    Upstream_irq::ack(ui);
    unmask();
    if (!Vkey::check_())
      kdb_ke("IRQ ENTRY");
  }
};


union Regs
{
#ifdef HAVE_PORTIO
  Static_object<L4::Io_register_block_port> io;
#endif
  Static_object<L4::Io_register_block_mmio> mem;
  Static_object<L4::Io_register_block_mmio_fixed_width<Unsigned64> > mem64;
  Static_object<L4::Io_register_block_mmio_fixed_width<Unsigned32> > mem32;
  Static_object<L4::Io_register_block_mmio_fixed_width<Unsigned16> > mem16;
};


bool
Kuart::setup_uart_io_port(void *r, Address base, int irq, bool resume)
{
#ifdef HAVE_PORTIO
  Regs *regs = static_cast<Regs *>(r);
  if (!resume)
    regs->io.construct(base);
  return startup(regs->io.get(), irq,
                 Koptions::o()->uart.base_baud, resume);
#else
  (void)r; (void)base; (void)irq; (void)resume;
  panic ("cannot use IO-Port based uart\n");
#endif
}

bool
Kuart::startup(unsigned, int irq, bool resume)
{
  static Regs regs;

  if (Koptions::o()->opt(Koptions::F_uart_base))
    {
      Address base = Koptions::o()->uart.base_address;
      switch (Koptions::o()->uart.access_type)
        {
        case Koptions::Uart_type_ioport:
          return setup_uart_io_port(&regs, base, irq, resume);

        case Koptions::Uart_type_mmio:
            {
              L4::Io_register_block *r = 0;

              // Koptions doesn't pass the UART size so take a sound guess.
              Address const size = 0x1000 - (base & 0xfff);
              switch (Koptions::o()->uart.reg_shift)
                {
                case 0: // no shift use natural access width
                  if (resume)
                    r = regs.mem;
                  else
                    r = regs.mem.construct(Kmem_mmio::map(base, size),
                                           Koptions::o()->uart.reg_shift);
                  break;
                case 1: // 1 bit shift, assume fixed 16bit access width
                  if (resume)
                    r = regs.mem16;
                  else
                    r = regs.mem16.construct(Kmem_mmio::map(base, size),
                                             Koptions::o()->uart.reg_shift);
                  break;
                case 2: // 2 bit shift, assume fixed 32bit access width
                  if (resume)
                    r = regs.mem32;
                  else
                    r = regs.mem32.construct(Kmem_mmio::map(base, size),
                                             Koptions::o()->uart.reg_shift);
                  break;
                case 3: // 3 bit shift, assume fixed 64bit access width
                  if (resume)
                    r = regs.mem64;
                  else
                    r = regs.mem64.construct(Kmem_mmio::map(base, size),
                                             Koptions::o()->uart.reg_shift);
                  break;
                default:
                  panic("UART: illegal reg shift value: %d",
                        Koptions::o()->uart.reg_shift);
                  break;
                }
              return startup(r, irq, Koptions::o()->uart.base_baud, resume);
            }
        default:
          return false;
        }
    }

  if (Koptions::o()->uart.access_type == Koptions::Uart_type_msr)
    return startup(0, irq, Koptions::o()->uart.base_baud, resume);

  return false;
}

} // anon namespace

FIASCO_INIT_SFX("kernel_uart")
bool
Kernel_uart::init_for_mode(Init_mode init_mode)
{
  if (Koptions::o()->uart.access_type == Koptions::Uart_type_ioport)
    return init_mode == Init_before_mmu;
  else
    return init_mode == Init_after_mmu;
}

FIASCO_CONST
Uart *
Kernel_uart::uart()
{ return _kernel_uart; }

void
Kernel_uart::pm_register()
{
  _kernel_uart->register_pm(Cpu_number::boot_cpu());
}

FIASCO_INIT_SFX("kernel_uart")
bool
Kernel_uart::init(Init_mode init_mode)
{
  if (!init_for_mode(init_mode))
    return false;

  if (Koptions::o()->opt(Koptions::F_noserial)) // do not use serial uart
    return true;

  _kernel_uart.construct();
  _fcon.construct(_kernel_uart);

  Kconsole::console()->register_console(_fcon, 0);
  return true;
}

void
Kernel_uart::enable_rcv_irq()
{
#ifdef CONFIG_INPUT
  static Kuart_irq uart_irq;
  auto mgr = Irq_mgr::mgr;
  if (mgr->alloc(&uart_irq, mgr->legacy_override(uart()->irq())))
    {
      uart_irq.unmask();
      uart()->enable_rx_irq(true);
    }
#endif
}

FIASCO_INIT_SFX("kernel_uart_input")
void
Kernel_uart::setup_input()
{
#ifdef CONFIG_INPUT
  // Do not touch Kernel_uart::uart() if serial support is disabled as a whole.
  // The object won't be constructed in this case.
  if (Koptions::o()->opt(Koptions::F_noserial))
    return;

  if ((Kernel_uart::uart()->failed()))
    return;

  int irq = -1;
  if (Config::serial_esc == Config::SERIAL_ESC_IRQ
      && (irq = Kernel_uart::uart()->irq()) == -1)
    {
      puts("SERIAL ESC: not supported");
      Config::serial_esc = Config::SERIAL_ESC_NOIRQ;
    }

  switch (Config::serial_esc)
    {
    case Config::SERIAL_ESC_NOIRQ:
      puts("SERIAL ESC: No IRQ for specified uart port.");
      puts("Using serial hack in slow timer handler.");
      break;

    case Config::SERIAL_ESC_IRQ:
      Kernel_uart::enable_rcv_irq();
      printf("SERIAL ESC: allocated IRQ %d for serial uart\n", irq);
      puts("Not using serial hack in slow timer handler.");
      break;
    }
#endif
}

