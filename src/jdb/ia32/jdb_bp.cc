
#include <jdb_bp.h>
#include <jdb.h>
#include <jdb_input_task.h>
#include <jdb_module.h>
#include <kmem.h>
#include <cstdio>

Mword Jdb_bp::dr7;

Breakpoint Jdb_bp::bps[4];

char const * const Breakpoint::mode_names[4] =
{
  "instruction", "write access", "i/o access", "r/w access"
};

void
Breakpoint::show()
{
  if (!addr.is_null())
    {
      printf("%5s on %12s at " L4_PTR_FMT,
	     log ? "LOG" : "BREAK", mode_names[mode & 3], addr.addr());
      if (mode != INSTRUCTION)
	printf(" len %d", len);
      else
	putstr("      ");

      if (   restrict.thread.thread == 0
	  && restrict.task.task == 0
	  && restrict.reg.reg == 0
	  && restrict.mem.len == 0)
	puts(" (not restricted)");
      else
	{
	  int j = 0;
#if 0
	  printf("\n%32s", "restricted to ");
	  if (restrict.thread.thread != (GThread_num)-1)
	    {
	      j++;
	      printf("thread%s %x.%x\n",
		     restrict.thread.other ? " !=" : "",
		     L4_uid::task_from_gthread (restrict.thread.thread),
		     L4_uid::lthread_from_gthread (restrict.thread.thread));
	    }
	  if (restrict.task.task)
	    {
	      if (j++)
		printf("%32s", "and ");
	      printf("task%s %p\n",
		     restrict.task.other ? " !=" : "",
		     restrict.task.task);
	    }
#endif
	  if (restrict.reg.reg != 0)
	    {
	      if (j++)
		printf("%32s", "and ");
	      printf("register %s in [" L4_PTR_FMT ", " L4_PTR_FMT "]\n",
                     (restrict.reg.reg > 0) && (restrict.reg.reg < 10)
                     ? Jdb_screen::Reg_names[restrict.reg.reg-1] : "???",
                     restrict.reg.y, restrict.reg.z);
	    }
	  if (restrict.mem.len != 0)
	    {
	      if (j++)
		printf("%32s", "and ");
	      printf("%d-byte var at " L4_PTR_FMT " in [" L4_PTR_FMT ", "
                     L4_PTR_FMT "]\n",
                     restrict.mem.len, restrict.mem.addr,
                     restrict.mem.y,   restrict.mem.z);
	    }
	}
    }
  else
    puts("disabled");
}

// return TRUE  if the breakpoint does NOT match
// return FALSE if all restrictions do match
int
Breakpoint::restricted(Thread *t)
{
  Jdb_entry_frame *e = Jdb::get_entry_frame(t->get_current_cpu());

  Space *task = t->space();
#if 0
  // investigate for thread restriction
  if (restrict.thread.thread != (GThread_num)-1)
    {
      if (restrict.thread.other ^ (restrict.thread.thread != t->id().gthread()))
	return 1;
    }

  // investigate for task restriction
  if (restrict.task.task)
    {
      if (restrict.task.other ^ (restrict.task.task != task))
	return 1;
    }
#endif
  // investigate for register restriction
  if (restrict.reg.reg)
    {
      Mword val = e->get_reg(restrict.reg.reg);
      Mword y   = restrict.reg.y;
      Mword z   = restrict.reg.z;

      // return true if rules do NOT match
      if (  (y <= z && (val <  y || val >  z))
	  ||(y >  z && (val >= z || val <= y)))
	return 1;
    }

  // investigate for variable restriction
  if (restrict.mem.len)
    {
      Mword val = 0;
      Mword y   = restrict.mem.y;
      Mword z   = restrict.mem.z;

      if (Jdb::peek_task(Jdb_address(restrict.mem.addr, task), &val, restrict.mem.len) != 0)
	return 0;

      // return true if rules do NOT match
      if (  (y <= z && (val <  y || val >  z))
	  ||(y >  z && (val >= z || val <= y)))
	return 1;
    }

  return 0;
}

int
Breakpoint::test_break(String_buffer *buf, Thread *t)
{
  if (restricted(t))
    return 0;

  buf->printf("break on %s at " L4_PTR_FMT, mode_names[mode], addr.addr());
  if (mode == WRITE || mode == ACCESS)
    {
      // If it's a write or access (read) breakpoint, we look at the
      // appropriate place and print the bytes we find there. We do
      // not need to look if the page is present because the x86 CPU
      // enters the debug exception immediately _after_ the memory
      // access was performed.
      Mword val = 0;
      if (len > sizeof(Mword))
	return 0;

      if (Jdb::peek_task(addr, &val, len) != 0)
	return 0;

      buf->printf(" [%08lx]", val);
    }
  return 1;
}

// Create log entry if breakpoint matches
void
Breakpoint::test_log(Thread *t)
{
  Jdb_entry_frame *e = Jdb::get_entry_frame(t->get_current_cpu());

  if (log && !restricted(t))
    {
      // log breakpoint
      Mword value = 0;

      if (mode == WRITE || mode == ACCESS)
	{
	  // If it's a write or access (read) breakpoint, we look at the
	  // appropriate place and print the bytes we find there. We do
	  // not need to look if the page is present because the x86 CPU
	  // enters the debug exception immediately _after_ the memory
	  // access was performed.
	  if (len > sizeof(Mword))
	    return;

	  if (Jdb::peek_task(addr, &value, len) != 0)
	    return;
	}

      // is called with disabled interrupts
      Tb_entry_bp *tb = static_cast<Tb_entry_bp*>(Jdb_tbuf::new_entry());
      tb->set(t, e->ip(), mode, len, value, addr.addr());
      Jdb_tbuf::commit_entry(tb);
    }
}


int
Jdb_bp::set_debug_address_register(int num, Jdb_address addr, Mword len,
				   Breakpoint::Mode mode)
{
  clr_dr7(num, dr7);
  set_dr7(num, len, mode, dr7);
  switch (num)
    {
    case 0: write_debug_register(0, addr.addr()); break;
    case 1: write_debug_register(1, addr.addr()); break;
    case 2: write_debug_register(2, addr.addr()); break;
    case 3: write_debug_register(3, addr.addr()); break;
    default:;
    }
  return 1;
}

void
Jdb_bp::at_jdb_enter()
{
  dr7 = read_debug_register(7);
  // disable breakpoints while we are in kernel debugger
  write_debug_register(7, dr7 & Jdb_bp_ia32_bits::Val_enter);
}

void
Jdb_bp::at_jdb_leave()
{
  write_debug_register(6, Jdb_bp_ia32_bits::Val_leave);
  write_debug_register(7, dr7);
}

/** @return 1 if single step occurred */
int
Jdb_bp::test_sstep(Cpu_number, Jdb_entry_frame *)
{
  Mword dr6 = read_debug_register(6);
  if (!(dr6 & Jdb_bp_ia32_bits::Val_test_sstep))
    return 0;

  // single step has highest priority, don't consider other conditions
  write_debug_register(6, Jdb_bp_ia32_bits::Val_leave);
  return 1;
}

/** @return 1 if breakpoint occurred */
int
Jdb_bp::test_break(Cpu_number cpu, Jdb_entry_frame *ef, String_buffer *buf)
{
  Mword dr6 = read_debug_register(6);
  if (!(dr6 & Jdb_bp_ia32_bits::Val_test))
    return 0;

  int ret = test_break(cpu, ef, buf, dr6);
  write_debug_register(6, dr6 & ~Jdb_bp_ia32_bits::Val_test);
  return ret;
}

// Return 1 if a breakpoint hits
int
Jdb_bp::test_break(Cpu_number cpu, Jdb_entry_frame *e, String_buffer *buf, Mword dr6)
{
  Thread *t = Jdb::get_thread(cpu);

  for (int i = 0; i < 4; i++)
    if (dr6 & (1 << i))
      {
	if (bps[i].break_at_instruction())
          e->flags(e->flags() | EFLAGS_RF);
	if (bps[i].test_break(buf, t))
	  return 1;
      }

  return 0;
}

/** @return 1 if other debug exception occurred */
int
Jdb_bp::test_other(Cpu_number, Jdb_entry_frame *, String_buffer *buf)
{
  Mword dr6 = read_debug_register(6);
  if (!(dr6 & Jdb_bp_ia32_bits::Val_test_other))
    return 0;

  buf->printf("unknown trap 1 (dr6=" L4_PTR_FMT ")", dr6);
  write_debug_register(6, Jdb_bp_ia32_bits::Val_leave);
  return 1;
}

/** @return 1 if only breakpoints were logged and jdb should not be entered */
int
Jdb_bp::test_log_only(Cpu_number, Jdb_entry_frame *)
{
  Mword dr6 = read_debug_register(6);

  if (dr6 & Jdb_bp_ia32_bits::Val_test)
    {
      dr7 = read_debug_register(7);
      // disable breakpoints -- we might trigger a r/w breakpoint again
      write_debug_register(7, dr7 & Jdb_bp_ia32_bits::Val_enter);
      test_log(dr6);
      write_debug_register(6, dr6);
      write_debug_register(7, dr7);
      if (!(dr6 & Jdb_bp_ia32_bits::Val_test_other))
	// don't enter jdb, breakpoints only logged
	return 1;
    }
  // enter jdb
  return 0;
}

void
Jdb_bp::init_arch()
{
  Jdb::bp_test_log_only = test_log_only;
  Jdb::bp_test_break    = test_break;
  Jdb::bp_test_sstep    = test_sstep;
  Jdb::bp_test_other    = test_other;
}

// Create log entry if breakpoint matches.
// Return 1 if debugger should stop
void
Jdb_bp::test_log(Mword &dr6)
{
  Thread *t = Jdb::get_thread(Cpu_number::boot_cpu());
  Jdb_entry_frame *e = Jdb::get_entry_frame(Cpu_number::boot_cpu());

  for (int i = 0; i < 4; i++)
    if (dr6 & (1 << i))
      {
	if (!bps[i].is_break())
	  {
	    // create log entry
	    bps[i].test_log(t);
	    // consider instruction breakpoints
	    if (bps[i].break_at_instruction())
	      e->flags(e->flags() | EFLAGS_RF);
	    // clear condition
	    dr6 &= ~(1 << i);
	  }
      }
}

Mword
Jdb_bp::test_match(Jdb_address addr, Breakpoint::Mode mode)
{
  for (int i = 0; i < 4; i++)
    if (bps[i].match_addr(addr, mode))
      return i + 1;

  return 0;
}

void
Jdb_bp::list()
{
  putchar('\n');

  for(int i = 0; i < 4; i++)
    {
      printf("  #%d: ", i + 1);
      bps[i].show();
    }

  putchar('\n');
}

int
Jdb_bp::set_breakpoint(int num, Jdb_address addr, Mword len,
		       Breakpoint::Mode mode, Breakpoint::Log log)
{
  if (set_debug_address_register(num, addr, len, mode))
    {
      bps[num].set(addr, len, mode, log);
      return 1;
    }

  return 0;
}

//---------------------------------------------------------------------------//
class Jdb_set_bp : public Jdb_module_mixin<Jdb_set_bp>, public Jdb_input_task_addr
{
public:
  Jdb_set_bp() FIASCO_INIT;
  Action_code action(int cmd, void *&args, char const *&fmt, int &next_char) override;

  static cxx::static_vector<Cmd const> jdb_cmds()
  {
    static Cmd cs[] =
      {
          { 0, "b", "bp", "%c",
            "b{i|a|w|p}<addr>\tset breakpoint on instruction/access/write/io "
            "access\n"
            "b{-|+|*}<num>\tdisable/enable/log breakpoint\n"
            "bl\tlist breakpoints\n"
            "br<num>{t|T|a|A|e|1|2|4}\trestrict breakpoint to "
            "(!)thread/(!)task/reg/mem",
            &breakpoint_cmd },
      };

    return cs;
  }


private:
  static char     breakpoint_cmd;
  static char     breakpoint_restrict_cmd;
  static Mword    breakpoint_number;
  static Mword    breakpoint_length;
  static Mword    breakpoint_restrict_task;
  static Mword    breakpoint_restrict_thread;
  typedef struct
    {
      char        reg;
      Mword       low;
      Mword       high;
    } Restrict_reg;
  static Restrict_reg breakpoint_restrict_reg;
  typedef struct
    {
      Address     addr;
      Mword       low;
      Mword       high;
    } Restrict_addr;
  static Restrict_addr breakpoint_restrict_addr;
  static int      state;
};

char     Jdb_set_bp::breakpoint_cmd;
char     Jdb_set_bp::breakpoint_restrict_cmd;
Mword    Jdb_set_bp::breakpoint_number;
Mword    Jdb_set_bp::breakpoint_length;
Mword    Jdb_set_bp::breakpoint_restrict_task;
Mword    Jdb_set_bp::breakpoint_restrict_thread;
Jdb_set_bp::Restrict_reg  Jdb_set_bp::breakpoint_restrict_reg;
Jdb_set_bp::Restrict_addr Jdb_set_bp::breakpoint_restrict_addr;
int      Jdb_set_bp::state;

Jdb_set_bp::Jdb_set_bp()
  : Jdb_module_mixin<Jdb_set_bp>("DEBUGGING")
{}

Jdb_module::Action_code
Jdb_set_bp::action(int cmd, void *&args, char const *&fmt, int &next_char)
{
  Jdb_module::Action_code code;
  Breakpoint::Mode mode;

  if (cmd == 0)
    {
      if (args == &breakpoint_cmd)
	{
	  switch (breakpoint_cmd)
	    {
	    case 'p':
	      if (!(Cpu::boot_cpu()->features() & FEAT_DE))
		{
		  puts(" I/O breakpoints not supported by this CPU");
		  return NOTHING;
		}
	      // fall through
	    case 'a':
	    case 'i':
	    case 'w':
	      if ((breakpoint_number = Jdb_bp::first_unused()) < 4)
		{
		  fmt   = " addr=%C";
		  args  = &Jdb_input_task_addr::first_char;
		  state = 1; // breakpoints are global for all tasks
		  return EXTRA_INPUT;
		}
	      puts(" No breakpoints available");
	      return NOTHING;
	    case 'l':
	      // show all breakpoints
	      Jdb_bp::list();
	      return NOTHING;
	    case '-':
	      // delete breakpoint
	    case '+':
	      // set logmode of breakpoint to <STOP>
	    case '*':
	      // set logmode of breakpoint to <LOG>
	    case 'r':
	      // restrict breakpoint
	      fmt   = " bpn=%1x";
	      args  = &breakpoint_number;
	      state = 2;
	      return EXTRA_INPUT;
	    case 't':
	      Jdb::execute_command("bt");
	      break;
	    default:
	      return ERROR;
	    }
	}
      else switch (state)
	{
	case 1:
	  code = Jdb_input_task_addr::action(args, fmt, next_char);
	  if (code == ERROR)
	    return ERROR;
	  if (code == NOTHING)
	    // ok, continue
	    goto got_address;
	  // more input for Jdb_input_task_addr
	  return code;
	case 2:
	  if (breakpoint_number < 1 || breakpoint_number > 4)
	    return ERROR;
	  // input is 1..4 but numbers are 0..3
	  breakpoint_number -= 1;
	  // we know the breakpoint number
	  switch (breakpoint_cmd)
	    {
	    case '-':
	      Jdb_bp::clr_breakpoint(breakpoint_number);
	      putchar('\n');
	      return NOTHING;
	    case '+':
	    case '*':
	      Jdb_bp::logmode_breakpoint(breakpoint_number, breakpoint_cmd);
	      putchar('\n');
	      return NOTHING;
	    case 'r':
	      fmt   = " %C";
	      args  = &breakpoint_restrict_cmd;
	      state = 5;
	      return EXTRA_INPUT;
	    default:
	      return ERROR;
	    }
	  break;
	case 3:
got_address:
	  // address/task read
	  if (breakpoint_cmd != 'i')
	    {
	      fmt   = " len (1, 2, 4...)=%1x";
	      args  = &breakpoint_length;
	      state = 4;
	      return EXTRA_INPUT;
	    }
	  breakpoint_length = 1; // must be 1 for instruction breakpoints
	  // fall through
	case 4:
	  // length read
	  if (breakpoint_length & (breakpoint_length - 1))
	    break;
	  if (breakpoint_length > sizeof(Mword))
	    break;
          switch (breakpoint_cmd)
	    {
	    default : return ERROR;
	    case 'i': mode = Breakpoint::INSTRUCTION; break;
	    case 'w': mode = Breakpoint::WRITE;       break;
	    case 'p': mode = Breakpoint::PORTIO;      break;
	    case 'a': mode = Breakpoint::ACCESS;      break;
	    }
	  // abort if no address was given
	  if (Jdb_input_task_addr::addr() == (Address)-1)
	    return ERROR;

	  Jdb_bp::set_breakpoint(breakpoint_number, Jdb_input_task_addr::address(),
				 breakpoint_length, mode, Breakpoint::BREAK);
	  putchar('\n');
	  break;
	case 5:
	  // restrict command read
	  switch (breakpoint_restrict_cmd)
	    {
	    case 'a':
	    case 'A':
	      fmt   = (breakpoint_restrict_cmd=='A')
			? "task!=" L4_ADDR_INPUT_FMT "\n"
                        : "task==" L4_ADDR_INPUT_FMT "\n";
	      args  = &breakpoint_restrict_task;
	      state = 6;
	      return EXTRA_INPUT;
	    case 't':
	    case 'T':
	      fmt   = (breakpoint_restrict_cmd=='T')
			? "thread!=%t\n" : "thread==%t\n";
	      args  = &breakpoint_restrict_thread;
	      state = 7;
	      return EXTRA_INPUT;
	    case 'e':
	      if (!Jdb::get_register(&breakpoint_restrict_reg.reg))
		return NOTHING;
	      fmt  = " in [" L4_ADDR_INPUT_FMT "-" L4_ADDR_INPUT_FMT "]\n";
              args = &breakpoint_restrict_reg.low;
	      state = 8;
	      return EXTRA_INPUT;
	    case '1':
	    case '2':
	    case '4':
	      putchar(breakpoint_restrict_cmd);
	      fmt   = "-byte addr=" L4_ADDR_INPUT_FMT
		      " between[" L4_ADDR_INPUT_FMT "-" L4_ADDR_INPUT_FMT "]\n";
	      args  = &breakpoint_restrict_addr;
	      state = 9;
	      return EXTRA_INPUT;
	    case '-':
	      Jdb_bp::clear_restriction(breakpoint_number);
	      putchar('\n');
	      break;
	    default:
	      return ERROR;
	    }
	  break;
	case 6:
	  // breakpoint restrict task read
	  Jdb_bp::restrict_task(breakpoint_number,
				breakpoint_restrict_cmd == 'A',
				breakpoint_restrict_task);
	  break;
	case 7:
	  // breakpoint restrict thread read
	  Jdb_bp::restrict_thread(breakpoint_number,
				  breakpoint_restrict_cmd == 'T',
				  breakpoint_restrict_thread);
	  break;
	case 8:
	  // breakpoint restrict register in range
	  Jdb_bp::restrict_register(breakpoint_number,
				    breakpoint_restrict_reg.reg,
				    breakpoint_restrict_reg.low,
				    breakpoint_restrict_reg.high);
	  break;
	case 9:
	  // breakpoint restrict x-byte-value in range
	  Jdb_bp::restrict_memory(breakpoint_number,
				  breakpoint_restrict_addr.addr,
				  breakpoint_restrict_cmd - '0',
				  breakpoint_restrict_addr.low,
				  breakpoint_restrict_addr.high);
	  break;
	}
    }
  return NOTHING;
}


static Jdb_set_bp jdb_set_bp INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
