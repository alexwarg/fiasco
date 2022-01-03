
#include "jdb_disasm.h"

#include <cstdio>
#include <cstring>
#include <cstdarg>

#include "disasm.h"
#include "jdb.h"
#include "jdb_bp.h"
#include "jdb_input.h"
#include "jdb_input_task.h"
#include "jdb_module.h"
#include "jdb_screen.h"
#include "kernel_console.h"
#include "keycodes.h"
#include "static_init.h"
#include "task.h"

char Jdb_disasm::show_intel_syntax;
char Jdb_disasm::show_arm_thumb;

bool
Jdb_disasm::disasm_line(char *buffer, int buflen, Jdb_address &addr)
{
  int len;

  if ((len = disasm_bytes(buffer, buflen, addr,
                          show_intel_syntax, show_arm_thumb, &Jdb::peek_task,
                          &Jdb::is_adapter_memory)) < 0)
    {
      addr += 1;
      return false;
    }

  addr += len;
  return true;
}

Address
Jdb_disasm::disasm_offset(Jdb_address &start, int offset)
{
  Jdb_address addr = start;
  if (offset > 0)
    {
      while (offset--)
        {
          if (!disasm_offset_incr(addr))
            {
              start = addr + offset;
              return false;
            }
        }
    }
  else
    {
      while (offset++)
        {
          if (!disasm_offset_decr(addr))
            {
              start = addr + offset - 1;
              return false;
            }
        }
    }
  start = addr;
  return true;
}

bool
Jdb_disasm::show_disasm_line(int len, Jdb_address &addr)
{
  int clreol = 0;
  if (len < 0)
    {
      len = -len;
      clreol = 1;
    }

  char line[len];
  if (disasm_line(line, len, addr))
    {
      if (clreol)
        printf("%.*s\033[K\n", len, line);
      else
        printf("%-*s\n", len, line);
      return true;
    }

  if (clreol)
    puts("........\033[K");
  else
    printf("........%*s", len-8, "\n");
  return false;
}

Jdb_module::Action_code
Jdb_disasm::show(Jdb_address virt, int level)
{
  Jdb_address enter_addr = virt;

  if (!level)
    Jdb::clear_screen();

  for (;;)
    {
      Jdb::cursor();

      Jdb_address addr;
      Mword i;
      for (i = Jdb_screen::height() - 1, addr = virt; i > 0; i--)
        {
          char stat_str[4] = { "   " };

          Kconsole::console()->getchar_chance();

          // show instruction breakpoint
#if defined(CONFIG_IA32) || defined(CONFIG_AMD64)
          if (Mword i = Jdb_bp::instruction_bp_at_addr(addr))
            {
              stat_str[0] = '#';
              stat_str[1] = '0' + i - 1;
            }
#endif

          printf("%s" L4_PTR_FMT "%s%s  ",
                 addr == enter_addr ? Jdb::esc_emph : "", addr.addr(), stat_str,
                 addr == enter_addr ? "\033[m" : "");
          show_disasm_line(
#ifdef CONFIG_BIT32
                           -64,
#else
                           -58,
#endif
                           addr);
        }

#if defined(CONFIG_IA32) || defined(CONFIG_AMD64)
      static char const *const syntax_mode[] = { "[AT&T]", "[Intel]" };
#elif defined(CONFIG_ARM)
      static char const *const arm_thumb[] = { "", "[thumb]" };
#endif
      char s[16];
      Jdb::printf_statline("dis",
                           "",
                           "<" L4_PTR_FMT "> %s  %-7s",
                           virt.addr(), Jdb::addr_space_to_str(virt, s, sizeof(s)),
#if defined(CONFIG_IA32) || defined(CONFIG_AMD64)
                           syntax_mode[(int)show_intel_syntax]
#elif defined(CONFIG_ARM)
                           arm_thumb[int{show_arm_thumb}]
#else
                           ""
#endif
                           );

      Jdb::cursor(Jdb_screen::height(), 6);
      switch (int c = Jdb_core::getchar())
        {
        case KEY_CURSOR_LEFT:
        case 'h':
          virt -= 1;
          break;
        case KEY_CURSOR_RIGHT:
        case 'l':
          virt += 1;
          break;
        case KEY_CURSOR_DOWN:
        case 'j':
          disasm_offset(virt, +1);
          break;
        case KEY_CURSOR_UP:
        case 'k':
          disasm_offset(virt, -1);
          break;
        case KEY_PAGE_UP:
        case 'K':
          disasm_offset(virt, -Jdb_screen::height() + 2);
          break;
        case KEY_PAGE_DOWN:
        case 'J':
          disasm_offset(virt, +Jdb_screen::height() - 2);
          break;
#if defined(CONFIG_IA32) || defined(CONFIG_AMD64)
        case KEY_TAB:
          show_intel_syntax ^= 1;
          break;
#elif defined(CONFIG_ARM) && defined(CONFIG_BIT32)
        case KEY_TAB:
          show_arm_thumb ^= 1;
          break;
#endif
        case KEY_CURSOR_HOME:
        case 'H':
          if (level > 0)
            return GO_BACK;
          break;
        case KEY_ESC:
          Jdb::abort_command();
          return NOTHING;
        default:
          if (Jdb::is_toplevel_cmd(c))
            return NOTHING;
          break;
        }
    }

  return GO_BACK;
}

Jdb_module::Action_code
Jdb_disasm::action(int cmd, void *&args, char const *&fmt, int &next_char)
{
  if (cmd == 0)
    {
      Jdb_module::Action_code code;

      code = Jdb_input_task_addr::action(args, fmt, next_char);
      if (code == Jdb_module::NOTHING
          && !Jdb_input_task_addr::address().is_null())
        {
          auto addr = Jdb_input_task_addr::address();
          return show(addr, 0) ? GO_BACK : NOTHING;
        }

      return code;
    }

  return NOTHING;
}

Jdb_module::Cmd const *
Jdb_disasm::cmds() const
{
  static Cmd cs[] =
    {
        { 0, "u", "u", "%C",
          "u[t<taskno>]<addr>\tdisassemble bytes of given/current task addr",
          &Jdb_input_task_addr::first_char }
    };

  return cs;
}

int
Jdb_disasm::num_cmds() const
{ return 1; }

Jdb_disasm::Jdb_disasm()
  : Jdb_module("INFO")
{}

static Jdb_disasm jdb_disasm INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

//----------------------------------------------------------------------------
#if defined(CONFIG_ARM) || defined(CONFIG_MIPS)

bool
Jdb_disasm::disasm_offset_decr(Jdb_address &addr)
{
  addr -= 4;
  return true;
}

bool
Jdb_disasm::disasm_offset_incr(Jdb_address &addr)
{
  addr += 4;
  return true;
}

#else

bool
Jdb_disasm::disasm_offset_decr(Jdb_address &addr)
{
  Jdb_address test_addr = addr - 64;
  Jdb_address work_addr;
  for (;;)
    {
      work_addr = test_addr;
      if (!disasm_line(0, 0, test_addr))
        return false;
      if (test_addr >= addr)
        break;
    }
  addr = work_addr;
  return true;
}

bool
Jdb_disasm::disasm_offset_incr(Jdb_address &addr)
{
  if (!disasm_line(0, 0, addr))
    return false;

  return true;
}

#endif

