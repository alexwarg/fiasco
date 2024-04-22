#pragma once

#include <irq_chip.h>
#include <thread.h>

#include <timer.h>
#include <system_clock.h>

#include <kdb_ke.h>
#include <kernel_console.h>
#include <vkey.h>

#include <globalconfig.h>

#ifdef CONFIG_JDB
#include <logdefs.h>
#include <string_buffer.h>
#include <tb_entry.h>
#endif

class Timer_tick_log
{
public:
#ifdef CONFIG_JDB
  struct Log : public Tb_entry
  {
    Address user_ip;
    void print(String_buffer *buf) const
    {
      buf->printf("u-ip=0x%lx", user_ip);
    }
  };

  static void log_timer()
  {
    Context *c = current();
    LOG_TRACE("Timer IRQs (kernel scheduling)", "timer", c, Log,
        l->user_ip  = c->regs()->ip();
    );
  }
#else
  static void log_timer() {}
#endif
};

template<typename TT>
class Timer_tick_base : public Irq_base, public Timer_tick_log
{
public:
  enum Mode
  {
    Any_cpu, ///< Might hit on any CPU
    Sys_cpu, ///< Hit only on the CPU that manages the system time
    App_cpu, ///< Hit only on application CPUs
  };

  Timer_tick_base() = default;

  /// Create a timer IRQ object
  explicit Timer_tick_base(Mode mode)
  {
    set_handler_mode(mode);
  }

  /// Create a timer IRQ object
  void set_handler_mode(Mode mode)
  {
    switch (mode)
      {
      case Any_cpu: set_hit(&handler_all); break;
      case Sys_cpu: set_hit(&handler_sys_time); break;
      case App_cpu: set_hit(&handler_app); break;
      }
  }

  /*
  static void setup(Cpu_number cpu);
  static void enable(Cpu_number cpu);
  static void disable(Cpu_number cpu);

  static Timer_tick_base *boot_cpu_timer_tick();
  */

  static void handler_all(Irq_base *_s, Upstream_irq const *ui)
  {
    Thread *t = current_thread();
    handle_timer(_s, ui, t, current_cpu());
  }

  static void handler_sys_time(Irq_base *_s, Upstream_irq const *ui)
  {
    // assume the boot CPU to be the CPU that manages the system time
    handle_timer(_s, ui, current_thread(), Cpu_number::boot_cpu());
  }

  static void handler_app(Irq_base *_s, Upstream_irq const *ui)
  {
    TT *self = nonull_static_cast<TT *>(_s);
    self->ack();
    Upstream_irq::ack(ui);
    log_timer();
    current_thread()->handle_timer_interrupt();
  }

private:
  // we do not support triggering modes
  void switch_mode(bool) override {}


  static void handle_timer(Irq_base *_s, Upstream_irq const *ui,
                           Thread *t, Cpu_number cpu)
  {
    TT *self = nonull_static_cast<TT *>(_s);
    self->ack();
    Upstream_irq::ack(ui);
    System_clock::update(cpu);
    if (   (cpu == Cpu_number::boot_cpu())
        && (Config::esc_hack || (Config::serial_esc == Config::SERIAL_ESC_NOIRQ)))
      {
        if (Kconsole::console()->char_avail() > 0 && !Vkey::check_())
          kdb_ke("SERIAL_ESC");
      }
    log_timer();
    t->handle_timer_interrupt();
  }
};

