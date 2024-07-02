
#pragma once

#include <cxx/atomic>
#include <cxx/function>
#include <cxx/type_traits>
#include <globalconfig.h>
#ifdef CONFIG_MP
#include "spin_lock.h"
#endif

#include "l4_types.h"
#include "cpu_mask.h"
#include "jdb_types.h"
#include "jdb_core.h"
#include "jdb_ansi.h"
#include "jdb_handler_queue.h"
#include "mem.h"
#include "per_cpu_data.h"
#include "processor.h"
#include "string_buffer.h"
#include "thread.h"

#include <jdb_monitored_mem.h>
#include <jdb_entry_frame.h>
#include <jdb_arch.h>

class Context;
class Space;
class Thread;
class Push_console;
class Trap_state;

class Jdb_entry_frame;

class Jdb : public Jdb_core, public Jdb_ansi, public Jdb_monitored_mem, public Jdb_arch
{
public:
  struct Remote_func : cxx::functor<void (Cpu_number)>
  {
    bool running;

    Remote_func() = default;
    Remote_func(Remote_func const &) = delete;
    Remote_func operator = (Remote_func const &) = delete;

    void reset_mp_safe()
    {
      set_monitored_address(&_f, Func{nullptr});
    }

    void set_mp_safe(cxx::functor<void (Cpu_number)> const &rf)
    {
      Remote_func const &f = static_cast<Remote_func const &>(rf);
      running = true;
      _d = f._d;
      Mem::mp_mb();
      set_monitored_address(&_f, f._f);
      Mem::mp_mb();
    }

    void monitor_exec(Cpu_number current_cpu)
    {
      if (Func f = monitor_address(current_cpu, &_f))
        {
          _f = 0;
          f(_d, cxx::forward<Cpu_number>(current_cpu));
          Mem::mp_mb();
          running = false;
        }
    }

    void wait() const
    {
      for (;;)
        {
          Mem::mp_mb();
          if (!running)
            break;
          Proc::pause();
        }
    }
  };

  static Per_cpu<Jdb_entry_frame*> entry_frame;
  static Cpu_number triggered_on_cpu;
  static Per_cpu<Remote_func> remote_func;

  static void init();

  static void write_tsc_s(String_buffer *buf, Signed64 tsc, bool sign);
  static void write_tsc(String_buffer *buf, Signed64 tsc, bool sign);

  static int FIASCO_FASTCALL enter_jdb(Trap_state *e, Cpu_number cpu);
  static void cursor_end_of_screen();
  static void cursor_home();
  static void printf_statline(const char *prompt, const char *help,
                              const char *format, ...)
  __attribute__((format(printf, 3, 4)));
  static void save_disable_irqs(Cpu_number cpu);
  static void restore_irqs(Cpu_number cpu);
  static void store_system_clock_on_enter()
  {
    if (!_system_clock_on_enter)
      _system_clock_on_enter = System_clock::clock();
  }

  static void clear_system_clock_on_enter()
  {
    _system_clock_on_enter = 0;
  }

  static Unsigned64 system_clock_on_enter()
  {
    return _system_clock_on_enter;
  }

  static Thread *get_thread(Cpu_number cpu);

  static Space *get_space(Cpu_number cpu)
  {
    Thread *thread = Jdb::get_thread(cpu);
    return thread ? thread->space() : nullptr;
  }

  template<typename THREAD>
  static Mword user_invoke_addr()
  {
    return reinterpret_cast<Mword>(&THREAD::user_invoke);
  }

private:
  Jdb();			// default constructors are undefined
  Jdb(const Jdb&);

  static char next_cmd;
  static bool was_input_error;

  static const char *toplevel_cmds;
  static const char *non_interactive_cmds;

  // state for traps in JDB itself
  static Per_cpu<bool> in_jdb;
  static bool in_service;
  static bool leave_barrier;
  static cxx::atomic<unsigned long> cpus_in_debugger;
  static bool never_break;
  static bool jdb_active;
  static Unsigned64 _system_clock_on_enter;

  static void enter_trap_handler(Cpu_number cpu);
  static void leave_trap_handler(Cpu_number cpu);
  static bool handle_conditional_breakpoint(Cpu_number cpu, Jdb_entry_frame *e);
  static void handle_nested_trap(Jdb_entry_frame *e);
  static bool handle_user_request(Cpu_number cpu);
  static bool handle_debug_traps(Cpu_number cpu);


public:
  static unsigned char *access_mem_task(Jdb_address addr, bool write);

  static Jdb_handler_queue jdb_enter;
  static Jdb_handler_queue jdb_leave;

  // esc sequences for highlighting
  static char  esc_iret[];
  static char  esc_bt[];
  static char  esc_emph[];
  static char  esc_emph2[];
  static char  esc_mark[];
  static char  esc_line[];
  static char  esc_symbol[];

  static char hide_statline;
  static Per_cpu<String_buf<81> > error_buffer;

  static bool cpu_in_jdb(Cpu_number cpu)
  { return Cpu::online(cpu) && in_jdb.cpu(cpu); }


  template< typename Func >
  static void foreach_cpu(Func &&f)
  {
    for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
      if (cpu_in_jdb(i))
        f(i);
  }

  template< typename Func >
  static bool foreach_cpu(Func const &f, bool positive)
  {
    bool r = positive;
    for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
      if (cpu_in_jdb(i))
        {
          bool res = f(i);

          if (positive)
            r = r && res;
          else
            r = r || res;
        }

    return r;
  }

  static Jdb_entry_frame *get_entry_frame(Cpu_number cpu)
  {
    return entry_frame.cpu(cpu);
  }

  static Space *get_task(Cpu_number cpu)
  {
    if (!get_thread(cpu))
      return nullptr;
    else
      return get_thread(cpu)->space();
  }

  static void set_next_cmd(char cmd)
  { next_cmd = cmd; }

  static int get_next_cmd()
  { return next_cmd; }

  static int getchar();

  /** Command aborted. If we are interpreting a debug command like
   *  enter_kdebugger("*#...") this is an error
   */
  static void abort_command()
  {
    cursor(Jdb_screen::height(), 6);
    clear_to_eol();

    was_input_error = true;
  }

  static bool is_toplevel_cmd(char c);
  static int execute_command(const char *s, int first_char = -1);
  static int execute_command_short(const char *s, int first_char = -1)
  {
    return execute_command_mode(true, s, first_char);
  }

  static int execute_command_long(const char *s, int first_char = -1)
  {
    return execute_command_mode(false, s, first_char);
  }

  static Push_console *push_cons();

  static void remote_work(Cpu_number cpu, cxx::functor<void (Cpu_number)> &&func,
                          bool sync = true);

  template<typename Func>
  static void on_each_cpu(Func &&func, bool single_sync = true)
  {
    foreach_cpu([&](Cpu_number cpu){ remote_work(cpu, cxx::forward<Func>(func), single_sync); });
  }

  template<typename Func>
  static void on_each_cpu_pl(Func &&func)
  {
    foreach_cpu([&](Cpu_number cpu){ remote_work(cpu, cxx::forward<Func>(func), false); });

    foreach_cpu([](Cpu_number cpu){ Jdb::remote_func.cpu(cpu).wait(); });
  }

  static void write_ll_ns(String_buffer *buf, Signed64 ns, bool sign);
  static void write_us_shortfmt(String_buffer *buf, Unsigned32 us);
  static void write_ll_hex(String_buffer *buf, Signed64 x, bool sign);
  static void write_ll_dec(String_buffer *buf, Signed64 x, bool sign);
  static void cpu_mask_print(Cpu_mask &m);
  static const char *addr_space_to_str(Jdb_address addr, char *str, size_t len);

  static int std_cursor_key(int c, Mword cols, Mword lines,
                            Mword max_absy, Mword max_pos,
                            Mword *absy, Mword *addy, Mword *addx, bool *redraw);

  static int peek_task(Jdb_address addr, void *value, size_t width);
  static int poke_task(Jdb_address addr, void const *value, size_t width);

  template< typename T >
  static bool peek(Jdb_addr<T> addr, cxx::remove_const_t<T> &value)
  {
    T tmp;
    bool ret = peek_task(addr, &tmp, sizeof(T)) == 0;
    value = tmp;
    return ret;
  }

  template< typename T >
  static bool poke(Jdb_addr<T> addr, T const &value)
  { return poke_task(addr, &value, sizeof(T)) == 0; }

  static void enter_getchar()
  {}

  static void leave_getchar()
  {}

private:
  static int execute_command_mode(bool is_short, const char *s, int first_char = -1);
  static int execute_command_ni(char const *str, int len);
  static int execute_command();
  static bool input_short_mode(Jdb::Cmd *cmd, char const **args, int &cmd_key);
  static bool input_long_mode(Jdb::Cmd *cmd, char const **args);
  static bool open_debug_console(Cpu_number cpu);
  static void close_debug_console(Cpu_number cpu);

  static void rcv_uart_enable();

#ifdef CONFIG_MP
  //---------------------------------------------------------------------------
  // remote call
  static Spin_lock<> _remote_call_lock;
  static void (*_remote_work_ipi_func)(Cpu_number, void *);
  static void *_remote_work_ipi_func_data;
  static unsigned long _remote_work_ipi_done;

  static int wait_for_app_cpus(Cpu_mask const &online_cpus_to_stop);
  static bool stop_all_cpus(Cpu_number current_cpu);
  static void force_app_cpus_into_jdb(bool try_nmi);
  static void leave_wait_for_others();
  static int remote_work_ipi_process(Cpu_number cpu);

  static void send_nmi(Cpu_number cpu);

public:
  static bool remote_work_ipi(Cpu_number this_cpu, Cpu_number to_cpu,
                              void (*f)(Cpu_number, void *), void *data, bool wait = true);

#else // CONFIG_MP
private:
  //--------------------------------------------------------------------------
  static bool stop_all_cpus(Cpu_number)
  { return true; }

  static void leave_wait_for_others()
  {}

  static void force_app_cpus_into_jdb(bool)
  {}

  static int remote_work_ipi_process(Cpu_number)
  { return 1; }
#endif

};
