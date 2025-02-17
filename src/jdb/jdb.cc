
#include <jdb.h>

#include <globalconfig.h>
#include <config.h>
#include <keycodes.h>
#include <trap_state.h>

#include <libc_backend.h>
#include <feature.h>

#include <ipi.h>
#include <logdefs.h>
#include <jdb_entry_frame.h>
#include <push_console.h>
#include <kernel_uart.h>
#include <kernel_console.h>
#include <paging_bits.h>

#include <ctype.h>


KIP_KERNEL_FEATURE("jdb");

Jdb_handler_queue Jdb::jdb_enter;
Jdb_handler_queue Jdb::jdb_leave;

char Jdb::esc_iret[]     = "\033[36;1m";
char Jdb::esc_bt[]       = "\033[31m";
char Jdb::esc_emph[]     = "\033[33;1m";
char Jdb::esc_emph2[]    = "\033[32;1m";
char Jdb::esc_mark[]     = "\033[35;1m";
char Jdb::esc_line[]     = "\033[37m";
char Jdb::esc_symbol[]   = "\033[33;1m";

DEFINE_PER_CPU Per_cpu<String_buf<81> > Jdb::error_buffer;
char Jdb::next_cmd;			// next global command to execute

char Jdb::hide_statline;		// show status line on enter_kdebugger
DEFINE_PER_CPU Per_cpu<Jdb_entry_frame*> Jdb::entry_frame;
Cpu_number Jdb::triggered_on_cpu = Cpu_number::nil(); // first CPU entered JDB
bool Jdb::was_input_error;		// error in command sequence

DEFINE_PER_CPU Per_cpu<Jdb::Remote_func> Jdb::remote_func;

// holds all commands executable in top level (regardless of current mode)
const char *Jdb::toplevel_cmds = "j_";

// a short command must be included in this list to be enabled for non-
// interactive execution
const char *Jdb::non_interactive_cmds = "bEIJLMNOPSU^Z*";

DEFINE_PER_CPU Per_cpu<bool> Jdb::running;	// JDB is already running
bool Jdb::never_break;		// never enter JDB
bool Jdb::jdb_active;
bool Jdb::in_service;
bool Jdb::leave_barrier;
cxx::atomic<unsigned long> Jdb::cpus_in_debugger;
Unsigned64 Jdb::_system_clock_on_enter;

void
Jdb::rcv_uart_enable()
{
  if (Config::serial_esc == Config::SERIAL_ESC_IRQ)
    Kernel_uart::enable_rcv_irq();
}

// go to bottom of screen and print some text in the form "jdb: ..."
// if no text follows after the prompt, prefix the current thread number
void
Jdb::printf_statline(const char *prompt, const char *help,
                     const char *format, ...)
{
  cursor(Jdb_screen::height(), 1);
  int w = Jdb_screen::width();
  prompt_start();
  if (prompt)
    {
      putstr(prompt);
      putstr(": ");
      w -= strlen(prompt) + 2;
    }
  else
    {
      Jdb::prompt();
      w -= Jdb::prompt_len();
    }
  prompt_end();
  // avoid -Wformat-zero-length warning
  if (format && (format[0] != '_' || format[1] != '\0'))
    {
      char buf[80];
      va_list list;
      va_start(list, format);
      vsnprintf(buf, sizeof(buf), format, list);
      va_end(list);
      putstr(buf);
      w -= print_len(buf);
    }
  if (help)
    {
      if (print_len(help) < w - 1)
        printf("%*.*s", w, w, help);
      else
        printf(" %*.*s...", w - 4, w - 4, help);
    }
  else
    clear_to_eol();
}

bool Jdb::is_toplevel_cmd(char c)
{
  char cm[] = {c, 0};
  Jdb_core::Cmd cmd = Jdb_core::has_cmd(cm);

  if (cmd.cmd || (0 != strchr(toplevel_cmds, c)))
    {
      set_next_cmd(c);
      return true;
    }

  return false;
}


int
Jdb::execute_command(const char *s, int first_char)
{
  Jdb_core::Cmd cmd = Jdb_core::has_cmd(s);

  if (cmd.cmd)
    {
      const char *args = 0;
      if (!short_mode)
        {
          args = s + strlen(cmd.cmd->cmd);
          while (isspace(*args))
            ++args;
        }
      return Jdb_core::exec_cmd(cmd, args, first_char) == 2 ? 1 : 0;
    }

  return 0;
}

int
Jdb::execute_command_mode(bool is_short, const char *s, int first_char)
{
  bool orig_mode = short_mode;
  short_mode = is_short;
  int r = execute_command(s, first_char);
  short_mode = orig_mode;
  return r;
}


Push_console *
Jdb::push_cons()
{
  static Push_console c;
  return &c;
}

// Interprete str as non interactive commands for Jdb. We allow mostly 
// non-interactive commands here (e.g. we don't allow d, t, l, u commands)
int
Jdb::execute_command_ni(char const *str, int len)
{
  for (; len && *str; ++str, --len)
    push_cons()->push(*str);

  push_cons()->push('_'); // terminating return

  bool leave = true;
  for (;;)
    {
      // Prevent output of sequences. Do this inside the loop because some
      // commands do Console::start_exclusive() + Console::end_exclusive().
      Kconsole::console()->change_state(0, 0, ~Console::OUTENABLED, 0);

      if (short_mode)
        {
          int c = getchar();
          if (c == KEY_RETURN || c == KEY_RETURN_2 || c == ' ')
            break;

          was_input_error = true;
          if (0 != strchr(non_interactive_cmds, c))
            {
              char _cmd[] = {static_cast<char>(c), 0};
              Jdb_core::Cmd cmd = Jdb_core::has_cmd(_cmd);

              if (cmd.cmd)
                {
                  if (Jdb_core::exec_cmd (cmd, 0) != 3)
                    was_input_error = false;
                }
            }

          if (was_input_error)
            {
              leave = false;
              break;
            }
        }
      else
        {
          Jdb_core::Cmd cmd(0, 0);
          char const *args;
          input_long_mode(&cmd, &args);
          if (!cmd.cmd)
            break;

          if (Jdb_core::exec_cmd(cmd, args) == 3)
            {
              leave = false;
              break;
            }
        }
    }

  push_cons()->flush();
  // re-enable all consoles but GZIP
  Kconsole::console()->change_state(0, Console::GZIP, ~0UL, Console::OUTENABLED);
  return leave;
}

bool
Jdb::input_short_mode(Jdb::Cmd *cmd, char const **args, int &cmd_key)
{
  *args = 0;
  for (;;)
    {
      int c;
      do
	{
	  if ((c = get_next_cmd()))
	    set_next_cmd(0);
	  else
	    c = getchar();
	}
      while (c < ' ' && c != KEY_RETURN && c != KEY_RETURN_2);

      if (c == KEY_F1)
	c = 'h';

      if (c >= 0x100) // see keycodes.h: no special keys on command line
        continue;

      printf("\033[K%c", c); // clreol + print key

      char cmd_buffer[2] = { static_cast<char>(c), 0 };

      *cmd = Jdb_core::has_cmd(cmd_buffer);
      if (cmd->cmd)
	{
	  cmd_key = c;
	  return false; // do not leave the debugger
	}
      else if (!handle_special_cmds(c))
	return true; // special command triggered a JDB leave
      else if (c == KEY_RETURN || c == KEY_RETURN_2)
	{
	  hide_statline = false;
	  cmd_key = c;
	  return false;
	}
      else
        {
          printf(" -- unknown command\n");
          return false;
        }
    }
}


class Cmd_buffer
{
private:
  unsigned  _l;
  char _b[256];

public:
  Cmd_buffer() {}
  char *buffer() { return _b; }
  int len() const { return _l; }
  void flush() { _l = 0; _b[0] = 0; }
  void cut(int l) 
  {
    if (l < 0)
      l = _l + l;

    if (l >= 0 && static_cast<unsigned>(l) < _l)
      {
	_l = l;
	_b[l] = 0;
      }
  }

  void append(int c) { if (_l + 1 < sizeof(_b)) { _b[_l++] = c; _b[_l] = 0; } }
  void append(char const *str, int len)
  {
    if (_l + len >= sizeof(_b))
      len = sizeof(_b) - _l - 1;

    memcpy(_b + _l, str, len);
    _l += len;
    _b[_l] = 0;
  }

  void overlay(char const *str, unsigned len)
  {
    if (len + 1 > sizeof(_b))
      len = sizeof(_b) - 1;

    if (len < _l)
      return;

    str += _l;
    len -= _l;

    memcpy(_b + _l, str, len);
    _l = len + _l;
  }

};


bool
Jdb::input_long_mode(Jdb::Cmd *cmd, char const **args)
{
  static Cmd_buffer buf;
  buf.flush();
  for (;;)
    {
      int c = getchar();

      switch (c)
	{
	case KEY_BACKSPACE:
	case KEY_BACKSPACE_2:
	  if (buf.len() > 0)
	    {
	      cursor(Cursor_left);
	      clear_to_eol();
	      buf.cut(-1);
	    }
	  continue;

	case KEY_TAB:
	    {
	      bool multi_match = false;
	      *cmd = Jdb_core::complete_cmd(buf.buffer(), multi_match);
	      if (cmd->cmd && multi_match)
		{
		  printf("\n");
		  unsigned prefix_len = Jdb_core::print_alternatives(buf.buffer());
		  print_prompt();
		  buf.overlay(cmd->cmd->cmd, prefix_len);
		  putnstr(buf.buffer(), buf.len());
		}
	      else if (cmd->cmd)
		{
		  putstr(cmd->cmd->cmd + buf.len());
		  putchar(' ');
		  buf.overlay(cmd->cmd->cmd, strlen(cmd->cmd->cmd));
		  buf.append(' ');
		}
	      continue;
	    }
	  break;

	case KEY_RETURN:
	case KEY_RETURN_2:
	  puts("");
	  if (!buf.len())
	    {
	      hide_statline = false;
	      cmd->cmd = 0;
	      return false;
	    }
	  break;

	default:
          if (c >= 0x100) // see keycodes.h: no special keys on command line
            continue;

	  buf.append(c);
	  printf("\033[K%c", c);
	  continue;
	}

      *cmd = Jdb_core::has_cmd(buf.buffer());
      if (cmd->cmd)
	{
	  unsigned cmd_len = strlen(cmd->cmd->cmd);
	  *args = buf.buffer() + cmd_len;
	  while (**args == ' ')
	    ++(*args);
	  return false; // do not leave the debugger
	}
      else
	{
	  printf("unknown command: '%s'\n", buf.buffer());
	  print_prompt();
	  buf.flush();
	}
    }
}

int
Jdb::execute_command()
{
  char const *args;
  Jdb_core::Cmd cmd(0,0);
  bool leave;
  int cmd_key = 0;

  if (short_mode)
    leave = input_short_mode(&cmd, &args, cmd_key);
  else
    leave = input_long_mode(&cmd, &args);

  if (leave)
    return 0;

  if (cmd.cmd)
    {
      int ret = Jdb_core::exec_cmd(cmd, args);

      if (!ret)
	hide_statline = false;

      return ret;
    }

  return 1;
}

bool
Jdb::open_debug_console(Cpu_number cpu)
{
  in_service = 1;
  __libc_backend_printf_local_force_unlock();
  save_disable_irqs(cpu);
  if (cpu == Cpu_number::boot_cpu())
    jdb_enter.execute();

  if (!stop_all_cpus(cpu))
    return false; // CPUs other than 0 never become interactive

  if (!Jdb_screen::direct_enabled())
    Kconsole::console()->
      change_state(Console::DIRECT, 0, ~Console::OUTENABLED, 0);

  return true;
}


void
Jdb::close_debug_console(Cpu_number cpu)
{
  Proc::cli();
  Mem::barrier();
  if (cpu == Cpu_number::boot_cpu())
    {
      running.cpu(cpu) = 0;
      // eat up input from console
      while (Kconsole::console()->getchar(false)!=-1)
	;

      Kconsole::console()->
	change_state(Console::DIRECT, 0, ~0UL, Console::OUTENABLED);

      in_service = 0;
      leave_wait_for_others();
      jdb_leave.execute();
    }

  Mem::barrier();
  restore_irqs(cpu);
}

void
Jdb::remote_work(Cpu_number cpu, cxx::functor<void (Cpu_number)> &&func,
                 bool sync)
{
  if (!Cpu::online(cpu))
    return;

  if (cpu == Cpu_number::boot_cpu())
    func(cpu);
  else
    {
      Jdb::Remote_func &rf = Jdb::remote_func.cpu(cpu);
      rf.wait();
      rf.set_mp_safe(func);

      if (sync)
        rf.wait();
    }
}

int
Jdb::getchar()
{
  int c = Jdb_core::getchar();
  check_for_cpus(false);
  return c;
}

void Jdb::cursor_home()
{
  putstr("\033[H");
}

void Jdb::cursor_end_of_screen()
{
  putstr("\033[127;1H");
}

//-------- pretty print functions ------------------------------
void
Jdb::write_ll_ns(String_buffer *buf, Signed64 ns, bool sign)
{
  Unsigned64 uns = (ns < 0) ? -ns : ns;

  if (uns >= 3'600'000'000'000'000ULL)
    {
      buf->printf(">999 h ");
      return;
    }

  if (sign)
    buf->printf("%c", (ns < 0) ? '-' : (ns == 0) ? ' ' : '+');

  if (uns >= 60'000'000'000'000ULL)
    {
      // 1000min...999h
      Mword _h  = uns / 3'600'000'000'000ULL;
      Mword _m  = (uns % 3'600'000'000'000ULL) / 60'000'000'000ULL;
      buf->printf("%3lu:%02lu h  ", _h, _m);
      return;
    }

  if (uns >= 1'000'000'000'000ULL)
    {
      // 1000s...999min
      Mword _m  = uns / 60'000'000'000ULL;
      Mword _s  = (uns % 60'000'000'000ULL) / 1'000ULL;
      buf->printf("%3lu:%02lu M  ", _m, _s);
      return;
    }

  if (uns >= 1'000'000'000ULL)
    {
      // 1...1000s
      Mword _s  = uns / 1'000'000'000ULL;
      Mword _ms = (uns % 1'000'000'000ULL) / 1'000'000ULL;
      buf->printf("%3lu.%03lu s ", _s, _ms);
      return;
    }

  if (uns >= 1'000'000)
    {
      // 1...1000ms
      Mword _ms = uns / 1'000'000UL;
      Mword _us = (uns % 1'000'000UL) / 1'000UL;
      buf->printf("%3lu.%03lu ms", _ms, _us);
      return;
    }

  if (uns == 0)
    {
      buf->printf("  0       ");
      return;
    }

  Mword _us = uns / 1'000UL;
  Mword _ns = uns % 1'000UL;
  buf->printf("%3lu.%03lu u ", _us, _ns);
}

void
Jdb::write_us_shortfmt(String_buffer *buf, Unsigned32 us)
{
  if (us >= 100'000'000)
    buf->printf(">99s");
  else if (us >= 10'000'000)
    buf->printf("%3us", us / 1'000'000);
  else if (us >= 1'000'000)
    buf->printf("%u.%us", us / 1'000'000, (us % 1'000'000) / 100'000);
  else if (us >= 10'000)
    buf->printf("%um", us / 1000);
  else if (us >= 1'000)
    buf->printf("%u.%um", us / 1000, (us % 1000) / 100);
  else
    buf->printf("%3uu", us);
}

void
Jdb::write_ll_hex(String_buffer *buf, Signed64 x, bool sign)
{
  // display 40 bits
  Unsigned64 xu = (x < 0) ? -x : x;

  if (sign)
    buf->printf("%s%03llx%08llx",
                (x < 0) ? "-" : (x == 0) ? " " : "+",
                (xu >> 32) & 0xfffU, xu & 0xffffffffU);
  else
    buf->printf("%04llx%08llx", (xu >> 32) & 0xffffU, xu & 0xffffffffU);
}

void
Jdb::write_ll_dec(String_buffer *buf, Signed64 x, bool sign)
{
  Unsigned64 xu = (x < 0) ? -x : x;

  // display no more than 11 digits
  if (xu >= 100000000000ULL)
    {
      buf->printf("%12s", ">= 10^11");
      return;
    }

  if (sign && x != 0)
    buf->printf("%+12lld", x);
  else
    buf->printf("%12llu", xu);
}

void
Jdb::cpu_mask_print(Cpu_mask &m)
{
  Cpu_number start = Cpu_number::nil();
  bool first = true;
  for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
    {
      if (m.get(i) && start == Cpu_number::nil())
        start = i;

      bool last = i + Cpu_number(1) == Config::max_num_cpus();
      if (start != Cpu_number::nil() && (!m.get(i) || last))
        {
          printf("%s%u", first ? "" : ",", cxx::int_value<Cpu_number>(start));
          first = false;
          if (i - Cpu_number(!last) > start)
            printf("-%u", cxx::int_value<Cpu_number>(i) - !(last && m.get(i)));

          start = Cpu_number::nil();
        }
    }
}

#if 0
__attribute__((weak)) void
Jdb::write_tsc_s(String_buffer *buf, Signed64 tsc, bool sign)
{
  Unsigned64 uns = Cpu::boot_cpu()->tsc_to_ns(tsc < 0 ? -tsc : tsc);

  if (tsc < 0)
    uns = -uns;

  if (sign)
    buf->printf("%c", (tsc < 0) ? '-' : (tsc == 0) ? ' ' : '+');

  Mword _s  = uns / 1000000000;
  Mword _us = (uns / 1000) - 1000000 * _s;
  buf->printf("%3lu.%06lu s ", _s, _us);
  return;
}

IMPLEMENT_DEFAULT
void
Jdb::write_tsc(String_buffer *buf, Signed64 tsc, bool sign)
{
  Unsigned64 ns = Cpu::boot_cpu()->tsc_to_ns(tsc < 0 ? -tsc : tsc);
  if (tsc < 0)
    ns = -ns;
  write_ll_ns(buf, ns, sign);
}
#endif

/// handling of standard cursor keys (Up/Down/PgUp/PgDn)
int
Jdb::std_cursor_key(int c, Mword cols, Mword lines,
                    Mword max_absy, Mword max_pos,
                    Mword *absy, Mword *addy, Mword *addx, bool *redraw)
{
  Mword old_absy = *absy;
  Mword old_pos  = (*absy + *addy) * cols + (addx ? *addx : 0);
  if (!max_pos)
    max_pos = (max_absy + lines-1) * cols-1;
  switch (c)
    {
    case KEY_CURSOR_LEFT:
    case 'h':
      if (!addx)
        return 0;
      if (*addx > 0)
        (*addx)--;
      else if (*addy > 0)
        (*addy)--, *addx = cols-1;
      else if (*absy > 0)
        (*absy)--, *addx = cols-1;
      break;
    case KEY_CURSOR_RIGHT:
    case 'l':
      if (!addx)
        return 0;
      if (*addx < cols-1 && old_pos+1 <= max_pos)
        (*addx)++;
      else if (*addy < lines-1)
        (*addy)++, *addx = 0;
      else if (*absy < max_absy)
        (*absy)++, *addx = 0;
      break;
    case KEY_CURSOR_UP:
    case 'k':
      if (*addy > 0)
        (*addy)--;
      else if (*absy > 0)
        (*absy)--;
      break;
    case KEY_CURSOR_DOWN:
    case 'j':
      if (*addy < lines-1 && old_pos + cols <= max_pos)
        (*addy)++;
      else if (*absy < max_absy && old_pos + cols <= max_pos)
        (*absy)++;
      else if (*absy < max_absy)
        (*absy)++, (*addy)--;
      break;
    case KEY_CURSOR_HOME:
    case 'H':
      if (addx)
        *addx = 0;
      *absy = 0;
      *addy = 0;
      break;
    case KEY_CURSOR_END:
    case 'L':
      if (addx)
        *addx = max_pos % cols;
      *absy = max_absy;
      *addy = lines-1;
      break;
    case KEY_PAGE_UP:
    case 'K':
      if (*absy >= lines)
        *absy -= lines;
      else if (*absy > 0)
        *absy = 0;
      else if (*addy > 0)
        *addy = 0;
      else if (addx)
        *addx = 0;
      break;
    case KEY_PAGE_DOWN:
    case 'J':
      if (*absy+lines-1 < max_absy && old_pos + lines * cols <= max_pos)
        *absy += lines;
      else if (*absy < max_absy)
        *absy = max_absy;
      else if (*addy < lines-1 && old_pos + (lines-1 - *addy) * cols <= max_pos)
        *addy = lines-1;
      else if (*addy < lines - 2)
        *addy = lines-2;
      else if (addx && old_pos + cols - 1 <= max_pos)
        *addx = cols - 1;
      else if (addx && *addy == lines - 1)
        *addx = max_pos % cols;
      break;
    default:
      return 0;
    }

  *redraw = *absy != old_absy;
  return 1;
}


//
// memory access wrappers
//

template < typename T >
static int
peek_or_poke_task(Jdb_address addr, T *value, size_t bytes)
{
  bool do_write = cxx::is_same_v<T, void const>;
  unsigned char *mem = Jdb::access_mem_task(addr, do_write);
  if (!mem)
    return -1;
  size_t bytes_to_copy = bytes;

  if (Pg::trunc(addr.addr()) != Pg::trunc(addr.addr() + bytes))
    bytes_to_copy = Pg::round(addr.addr()) - addr.addr();
  if constexpr (cxx::is_same_v<T, void>)
    memcpy(value, mem, bytes_to_copy);
  else
    {
      memcpy(mem, value, bytes_to_copy);
      Mem_unit::make_coherent_to_pou(mem, bytes_to_copy);
    }

  if (bytes_to_copy != bytes)
    {
      mem = Jdb::access_mem_task(addr, do_write);
      if (!mem)
        return -1;
      addr += bytes_to_copy;
      bytes_to_copy = bytes - bytes_to_copy;
      if constexpr (cxx::is_same_v<T, void>)
        memcpy(value, mem, bytes_to_copy);
      else
        {
          memcpy(mem, value, bytes_to_copy);
          Mem_unit::make_coherent_to_pou(mem, bytes_to_copy);
        }
    }
  return 0;
}

int
Jdb::peek_task(Jdb_address addr, void *value, size_t width)
{ return peek_or_poke_task(addr, value, width); }

int
Jdb::poke_task(Jdb_address addr, void const *value, size_t width)
{ return peek_or_poke_task(addr, value, width); }


class Jdb_base_cmds : public Jdb_module
{
public:
  Jdb_base_cmds() FIASCO_INIT;

  Action_code action (int cmd, void *&, char const *&, int &) override
  {
    if (cmd!=0)
      return NOTHING;

    Jdb_core::short_mode = !Jdb_core::short_mode;
    printf("\ntoggle mode: now in %s command mode (use '%s' to switch back)\n",
           Jdb_core::short_mode ? "short" : "long",
           Jdb_core::short_mode ? "*" : "mode");
    return NOTHING;
  }

  int num_cmds() const override
  {
    return 1;
  }

  Cmd const *cmds() const override
  {
    static Cmd cs[] =
      { { 0, "*", "mode", "", "*|mode\tswitch long and short command mode",
          (void*)0 } };

    return cs;
  }

};

static Jdb_base_cmds jdb_base_cmds INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

Jdb_base_cmds::Jdb_base_cmds()
  : Jdb_module("GENERAL")
{}

int
Jdb::enter_jdb(Trap_state *ts, Cpu_number cpu)
{
  static_assert(sizeof(Jdb_entry_frame) == sizeof(Trap_state));
  auto *e = static_cast<Jdb_entry_frame *>(ts);

  if (e->debug_ipi())
    {
      if (!remote_work_ipi_process(cpu))
        return 0;
      if (!in_service)
	return 0;
    }

  enter_trap_handler(cpu);

  if (handle_conditional_breakpoint(cpu, e))
    {
      // don't enter debugger, only logged breakpoint
      leave_trap_handler(cpu);
      return 0;
    }

  if (!running.cpu(cpu))
    entry_frame.cpu(cpu) = e;

  volatile bool really_break = true;

  static jmp_buf recover_buf;
  static Jdb_entry_frame nested_trap_frame;

  if (running.cpu(cpu))
    {
      nested_trap_frame = *e;

      // Since we entered the kernel debugger a second time,
      // Thread::nested_trap_recover
      // has a value of 2 now. We don't leave this function so correct the
      // entry counter
      Thread::nested_trap_recover.cpu(cpu)--;

      longjmp(recover_buf, 1);
    }

  // all following exceptions are handled by jdb itself
  running.cpu(cpu) = true;

  // If entered by Ipi::Debug, a thread on another CPU requested this CPU to
  // enter. Otherwise, we entered by exception. Remember the first one.
  if (!e->debug_ipi() && triggered_on_cpu == Cpu_number::nil())
    triggered_on_cpu = cpu;

  if (!open_debug_console(cpu))
    { // not on the master CPU just wait
      close_debug_console(cpu);
      leave_trap_handler(cpu);
      return 0;
    }

  // As of here, we are certain that this is the boot CPU!

  store_system_clock_on_enter();

  if (triggered_on_cpu == Cpu_number::nil())
    triggered_on_cpu = Cpu_number::boot_cpu(); // should not happen

  // check for kdb_ke debugging interface; only used from kernel context
  if (foreach_cpu(&handle_user_request, true))
    {
      close_debug_console(cpu);
      leave_trap_handler(cpu);
      triggered_on_cpu = Cpu_number::nil();
      clear_system_clock_on_enter();
      return 0;
    }

  hide_statline = false;

  error_buffer.cpu(cpu).clear();

  really_break = foreach_cpu(&handle_debug_traps, false);

  while (setjmp(recover_buf))
    {
      // handle traps which occurred while we are in Jdb
      Kconsole::console()->end_exclusive(Console::GZIP);
      handle_nested_trap(&nested_trap_frame);
    }

  if (!never_break && really_break) 
    {
      do
	{
	  screen_scroll(1, Jdb_screen::height());
	  if (!hide_statline)
	    {
	      cursor(Jdb_screen::height(), 1);
	      printf("\n%s%s    %.*s\033[m      \n",
	             esc_prompt,
	             test_checksums()
	               ? ""
	               : "    WARNING: Fiasco kernel checksum differs -- "
	                 "read-only data has changed!\n",
	             Jdb_screen::width() - 11,
	             Jdb_screen::Line);

              Cpu_mask cpus_in_jdb;
              int cpu_cnt = 0;
	      for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
		if (Cpu::online(i))
		  {
		    if (running.cpu(i))
                      {
                        ++cpu_cnt;
                        cpus_in_jdb.set(i);
                        if (!entry_frame.cpu(i)->debug_ipi())
                          printf("    CPU%2u [" L4_PTR_FMT "]: %s\n",
                                 cxx::int_value<Cpu_number>(i),
                                 entry_frame.cpu(i)->ip(),
                                 error_buffer.cpu(i).c_str());
                      }
		    else
		      printf("    CPU%2u: is not in JDB (not responding)\n",
                             cxx::int_value<Cpu_number>(i));
		  }
              if (!cpus_in_jdb.empty() && cpu_cnt > 1)
                {
                  printf("    CPU(s) ");
                  cpu_mask_print(cpus_in_jdb);
                  printf(" entered JDB\n");
                }
	      hide_statline = true;
	    }

	  printf_statline(0, 0, "_");

	} while (execute_command());

      // reset scrolling region of serial terminal
      screen_scroll(1,127);

      // reset cursor
      blink_cursor(Jdb_screen::height(), 1);

      // goto end of screen
      Jdb::cursor(127, 1);
    }

  // re-enable interrupts
  triggered_on_cpu = Cpu_number::nil();
  close_debug_console(cpu);

  rcv_uart_enable();

  clear_system_clock_on_enter();
  leave_trap_handler(cpu);
  return 0;
}

const char *
Jdb::addr_space_to_str(Jdb_address addr, char *str, size_t len)
{
  if (addr.is_kmem())
    return "kernel";
  if (addr.is_phys())
    return "physical";
  snprintf(str, len, "task D:%lx",
           static_cast<Task*>(addr.space())->dbg_info()->dbg_id());
  return str;
}


#ifdef CONFIG_MP
//--------------------------------------------------------------------------

#include <delayloop.h>


void (*Jdb::_remote_work_ipi_func)(Cpu_number, void *);
void *Jdb::_remote_work_ipi_func_data;
unsigned long Jdb::_remote_work_ipi_done;
Spin_lock<> Jdb::_remote_call_lock;

bool
Jdb::check_for_cpus(bool try_nmi)
{
  enum { Max_wait_cnt = 1000 };
  for (Cpu_number c = Cpu_number::second(); c < Config::max_num_cpus(); ++c)
    {
      if (Cpu::online(c) && !running.cpu(c))
	Ipi::send(Ipi::Debug, Cpu_number::first(), c);
    }
  Mem::barrier();
retry:
  unsigned long wait_cnt = 0;
  for (;;)
    {
      bool all_there = true;
      cpus_in_debugger.store(0);
      // skip boot cpu 0
      for (Cpu_number c = Cpu_number::second(); c < Config::max_num_cpus(); ++c)
	{
	  if (Cpu::online(c))
	    {
	      if (!running.cpu(c))
		all_there = false;
	      else
		cpus_in_debugger.fetch_add(1);
	    }
	}

      if (!all_there)
	{
	  Proc::pause();
	  Mem::barrier();
	  if (++wait_cnt == Max_wait_cnt)
	    break;
	  Delay::delay(1);
	  continue;
	}

      break;
    }

  bool do_retry = false;
  for (Cpu_number c = Cpu_number::second(); c < Config::max_num_cpus(); ++c)
    {
      if (Cpu::online(c))
	{
	  if (!running.cpu(c))
	    {
	      printf("JDB: CPU%u: is not responding ... %s\n",
                     cxx::int_value<Cpu_number>(c),
		     try_nmi ? "trying NMI" : "");
	      if (try_nmi)
		{
		  do_retry = true;
		  send_nmi(c);
		}
	    }
	}
    }
  if (do_retry)
    {
      try_nmi = false;
      goto retry;
    }
  // All CPUs entered JDB, so go on and become interactive
  return true;
}

bool
Jdb::stop_all_cpus(Cpu_number current_cpu)
{
  enum { Max_wait_cnt = 1000 };
  // JDB always runs on the boot CPU, if any other CPU enters the debugger
  // the boot CPU is notified to do enter the debugger too
  if (current_cpu == Cpu_number::boot_cpu())
    {
      // I'm CPU 0 stop all other CPUs and wait for them to enter the JDB
      jdb_active = 1;
      Mem::barrier();
      check_for_cpus(true);
      // All CPUs entered JDB, so go on and become interactive
      return true;
    }
  else
    {
      // Huh, not CPU 0, so notify CPU 0 to enter JDB too
      // The notification is ignored if CPU 0 is already within JDB
      jdb_active = true;
      Ipi::send(Ipi::Debug, current_cpu, Cpu_number::boot_cpu());

      unsigned long wait_count = Max_wait_cnt;
      while (!running.cpu(Cpu_number::boot_cpu()) && wait_count)
	{
	  Proc::pause();
          Delay::delay(1);
	  Mem::barrier();
	  --wait_count;
	}

      if (wait_count == 0)
	send_nmi(Cpu_number::boot_cpu());

      // Wait for messages from CPU 0
      while (access_once(&jdb_active))
	{
	  Mem::mp_mb();
          remote_func.cpu(current_cpu).monitor_exec(current_cpu);
	  Proc::pause();
	}

      // This CPU defacto left JDB
      running.cpu(current_cpu) = 0;

      // Signal CPU 0, that we are ready to leve the debugger
      // This is the second door of the airlock
      cpus_in_debugger.fetch_sub(1UL);

      // Wait for CPU 0 to leave us out
      while (access_once(&leave_barrier))
	{
	  Mem::barrier();
	  Proc::pause();
	}

      // CPU 0 signaled us to leave JDB
      return false;
    }
}

void
Jdb::leave_wait_for_others()
{
  leave_barrier = 1;
  jdb_active = 0;
  Mem::barrier();
  for (;;)
    {
      bool all_there = true;
      for (Cpu_number c = Cpu_number::first(); c < Config::max_num_cpus(); ++c)
	{
	  if (cpu_in_jdb(c))
	    {
	      // notify other CPU
              Jdb::remote_func.cpu(c).reset_mp_safe();
//	      printf("JDB: wait for CPU[%2u] to leave\n", cxx::int_value<Cpu_number>(c));
	      all_there = false;
	    }
	}

      if (!all_there)
	{
	  Proc::pause();
	  Mem::mp_mb();
	  continue;
	}

      break;
    }

  while (cpus_in_debugger.load())
    Proc::pause();

  Mem::barrier();
  leave_barrier = 0;
}

// The remote_work_ipi* functions are for the IPI round-trip benchmark (only)
int
Jdb::remote_work_ipi_process(Cpu_number cpu)
{
  if (_remote_work_ipi_func)
    {
      _remote_work_ipi_func(cpu, _remote_work_ipi_func_data);
      Mem::barrier();
      _remote_work_ipi_done = 1;
      return 0;
    }
  return 1;
}

bool
Jdb::remote_work_ipi(Cpu_number this_cpu, Cpu_number to_cpu,
                     void (*f)(Cpu_number, void *), void *data, bool wait)
{
  if (to_cpu == this_cpu)
    {
      f(this_cpu, data);
      return true;
    }

  if (!Cpu::online(to_cpu))
    return false;

  auto guard = lock_guard(_remote_call_lock);

  _remote_work_ipi_func      = f;
  _remote_work_ipi_func_data = data;
  _remote_work_ipi_done      = 0;

  Ipi::send(Ipi::Debug, this_cpu, to_cpu);

  if (wait)
    while (!access_once(&_remote_work_ipi_done))
      Proc::pause();

  _remote_work_ipi_func = 0;

  return true;
}

#endif
