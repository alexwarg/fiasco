
#include "jdb_ptab.h"

#include <cstdio>

#include "config.h"
#include "jdb.h"
#include "jdb_disasm.h"
#include "jdb_kobject.h"
#include "jdb_module.h"
#include "jdb_screen.h"
#include "jdb_table.h"
#include "kernel_console.h"
#include "kernel_task.h"
#include "kmem.h"
#include "keycodes.h"
#include "space.h"
#include "task.h"
#include "thread.h"
#include "static_init.h"
#include "types.h"

char Jdb_ptab_m::first_char;

Jdb_ptab::Jdb_ptab(void *pt_base, Space *task,
                   unsigned char pt_level, unsigned entries,
                   Address virt_base, int level)
  : base((Address)pt_base), virt_base(virt_base), _level(level),
    _task(task), entries(entries), cur_pt_level(pt_level), dump_raw(0)
{
  if (entries == 0)
    this->entries = Pdir::Levels::length(pt_level);
}

unsigned
Jdb_ptab::col_width(unsigned column) const
{
  if (column == 0)
    return Jdb_screen::Col_head_size;
  else
    return sizeof(Pdir::Pte_ptr::Entry) * 2;
}

unsigned long
Jdb_ptab::cols() const
{
  return Jdb_screen::cols(sizeof(Mword) * 2, sizeof(Pdir::Pte_ptr::Entry) * 2);
}

unsigned long
Jdb_ptab::rows() const
{
  if (cols() > 1)
    return (entries + cols() - 2) / (cols()-1);
  return 0;
}

// available from the jdb_dump module
int jdb_dump_addr_task(Jdb_address addr, int level)
  __attribute__((weak));

void
Jdb_ptab::draw_entry(unsigned long row, unsigned long col)
{
  int idx;
  if (col == 0)
    {
      idx = index(row, 1);
      if (idx >= 0)
        print_head(pte(idx));
      else
        putstr("        ");
    }
  else if ((idx = index(row, col)) >= 0)
    print_entry(Pdir::Pte_ptr(pte(idx), cur_pt_level));
  else
    print_invalid();
}

Address
Jdb_ptab::entry_phys(Pdir::Pte_ptr const &entry)
{
  if (!entry.is_leaf())
    return entry.next_level();

  return entry.page_addr();
}

 __attribute__((weak))
void *
Jdb_ptab::entry_virt(Pdir::Pte_ptr const &entry)
{
  return (void*)Mem_layout::phys_to_pmem(entry_phys(entry));
}

unsigned
Jdb_ptab::entry_is_pt_ptr(Pdir::Pte_ptr const &entry,
                           unsigned *entries, unsigned *next_level)
{
  if (!entry.is_valid() || entry.is_leaf())
    return 0;

  unsigned n = 1;
  while (   (entry.level + n) < Pdir::Depth
         && Pdir::Levels::length(entry.level + n) <= 1)
    ++n;

  *entries = Pdir::Levels::length(entry.level + n);
  *next_level = entry.level + n;
  return 1;
}

void
Jdb_ptab::print_head(void *entry)
{
  printf(L4_PTR_FMT, (Address)entry);
}

Address
Jdb_ptab::disp_virt(int idx)
{
  Pdir::Va e((Mword)idx << Pdir::lsb_for_level(cur_pt_level));
  return cxx::int_value<Virt_addr>(e) + virt_base;
}

void
Jdb_ptab::print_statline(unsigned long row, unsigned long col)
{
  unsigned long sid = Kobject_dbg::pointer_to_id(_task);

  Address va;
  int idx = index(row, col);
  if (idx >= 0)
    va = disp_virt(idx);
  else
    va = -1;

  Jdb::printf_statline("p:", "<Space>=mode <CR>=goto page/next level",
                       "<level=%1d> <" L4_PTR_FMT "> task D:%lx", cur_pt_level, va, sid);
}

unsigned
Jdb_ptab::key_pressed(int c, unsigned long &row, unsigned long &col)
{
  switch (c)
    {
    default:
      return Nothing;

    case KEY_CURSOR_HOME: // return to previous or go home
      if (_level == 0)
        return Nothing;
      return Back;

    case ' ':
      dump_raw ^= 1;
      return Redraw;

    case 'u': // disassemble using address the cursor points to
      if (Jdb_disasm::avail() && _level<=7)
        {
          int idx = index(row, col);
          if (idx < 0)
            break;

          Pdir::Pte_ptr pt_entry(pte(idx), cur_pt_level);
          if (!pt_entry.is_valid())
            break;

          unsigned next_level, entries;

          if (cur_pt_level >= Pdir::Depth ||
              !entry_is_pt_ptr(pt_entry, &entries, &next_level))
            {
              Jdb_address virt(disp_virt(idx), _task);
              char s[16];
              Jdb::printf_statline("p", "<CR>=disassemble here",
                                   "u[address=" L4_PTR_FMT " %s] ", virt.addr(),
                                   Jdb::addr_space_to_str(virt, s, sizeof(s)));
              int c1 = Jdb_core::getchar();
              if (c1 != KEY_RETURN && c1 != ' ' && c != KEY_RETURN_2)
                {
                  Jdb::printf_statline("p", 0, "u");
                  Jdb::execute_command("u", c1);
                  return Exit;
                }

              return Jdb_disasm::show(virt, _level + 1)
                ? Redraw
                : Exit;
            }
        }
      return Handled;

    case KEY_RETURN:	// goto ptab/address under cursor
    case KEY_RETURN_2:
      if (_level<=7)
        {
          int idx = index(row, col);
          if (idx < 0)
            break;

          Pdir::Pte_ptr pt_entry(pte(idx), cur_pt_level);
          if (!pt_entry.is_valid())
            break;

          void *pd_virt = entry_virt(pt_entry);

          unsigned next_level, entries;

          if (cur_pt_level < Pdir::Depth
              && entry_is_pt_ptr(pt_entry, &entries, &next_level))
            {
              Jdb_ptab pt_view(pd_virt, _task, next_level, entries,
                               disp_virt(idx), _level+1);
              if (!pt_view.show(0,1))
                return Exit;
              return Redraw;
            }
          else if (jdb_dump_addr_task != 0)
            {
              if (!jdb_dump_addr_task(Jdb_address(disp_virt(idx), _task), _level + 1))
                return Exit;
              return Redraw;
            }
        }
      break;
    }

  return Handled;
}

#ifdef CONFIG_BIT32
void Jdb_ptab::print_invalid() { putstr("   ###  "); }
#else
void Jdb_ptab::print_invalid() { putstr("       ###      "); }
#endif


bool
Jdb_ptab_m::handle_key(Kobject_common *o, int code)
{
  if (code != 'p')
    return false;

  Space *t = cxx::dyn_cast<Task*>(o);
  if (!t)
    {
      Thread *th = cxx::dyn_cast<Thread*>(o);
      if (!th || !th->space())
        return false;

      t = th->space();
    }

  Jdb_ptab pt_view(static_cast<Mem_space*>(t)->dir(), t, 0, 0, 0, 1);
  pt_view.show(0,0);

  return true;
}

char const *
Jdb_ptab_m::help_text(Kobject_common *o) const
{
  Thread *t;
  if (cxx::dyn_cast<Task*>(o) || ((t = cxx::dyn_cast<Thread*>(o)) && t->space()))
    return "p=ptab";

  return 0;
}

Jdb_module::Action_code
Jdb_ptab_m::action(int cmd, void *&args, char const *&fmt, int &next_char)
{
  if (cmd == 0)
    {
      if (args == &first_char)
        {
          if (first_char != KEY_RETURN && first_char != KEY_RETURN_2
              && first_char != ' ')
            {
              fmt       = "%q";
              args      = &task;
              next_char = first_char;
              return EXTRA_INPUT_WITH_NEXTCHAR;
            }
          else
            task = 0; // use current task -- see below
        }
      else if (args == &task)
        {
        }
      else
        return NOTHING;

      Space *s;
      if (task)
        {
          s = cxx::dyn_cast<Task*>(reinterpret_cast<Kobject*>(task));
          if (!s)
            return Jdb_module::NOTHING;
        }
      else
        s = Jdb::get_space(Jdb::current_cpu);

      void *ptab_base;
      if (!(ptab_base = ((void*)static_cast<Mem_space*>(s)->dir())))
        return Jdb_module::NOTHING;

      Jdb::clear_screen();
      Jdb_ptab pt_view(ptab_base, s);
      pt_view.show(0,1);
    }

  return NOTHING;
}

Jdb_module::Cmd const *
Jdb_ptab_m::cmds() const
{
  static Cmd cs[] =
    {
        { 0, "p", "ptab", "%C",
          "p[<taskno>]\tshow pagetable of current/given task",
          &first_char },
    };
  return cs;
}

int
Jdb_ptab_m::num_cmds() const
{
  return 1;
}

Jdb_ptab_m::Jdb_ptab_m()
  : Jdb_module("INFO")
{
  Jdb_kobject::module()->register_handler(this);
}

static Jdb_ptab_m jdb_ptab_m INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
