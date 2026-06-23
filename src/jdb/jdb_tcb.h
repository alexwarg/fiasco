#pragma once

#include <jdb_tcb_ptr.h>
#include <jdb_entry_frame.h>
#include <jdb_screen.h>
#include <jdb_module.h>
#include <jdb_kobject.h>
#include <jdb_types.h>
#include <jdb_disasm.h>
#include <jdb.h>
#include <types.h>

int jdb_show_tcb(Thread* thread, int level);

class Jdb_disasm_view
{
public:
  unsigned _x, _y;

  Jdb_disasm_view(unsigned x, unsigned y)
  : _x(x), _y(y)
  {}

  void show(Jdb_address addr, bool dump_only)
  {
    if (!Jdb_disasm::avail())
      return;

    Jdb_address disass_addr = addr;
    if (dump_only)
      {
        for (unsigned i = 0; i < 20; ++i)
          Jdb_disasm::show_disasm_line(Jdb_screen::width(), disass_addr);
        return;
      }

    Jdb::cursor(_y, _x);
    putstr(Jdb::esc_emph);
    Jdb_disasm::show_disasm_line(-40, disass_addr);
    putstr("\033[m");
    Jdb::cursor(_y + 1, _x);
    Jdb_disasm::show_disasm_line(-40, disass_addr);
  }
};


class Jdb_stack_view
{
public:
  bool is_current;
  Jdb_entry_frame *ef;
  Jdb_tcb_ptr current;
  unsigned start_y;
  Address absy;
  Address addy, addx;
  bool memdump_is_colored;

  bool edit_registers();

  void dump(bool dump_only, Address ksp);
  void highlight(bool highl);

  Jdb_stack_view(unsigned y, int show_obj_help = 1)
  : start_y(y), absy(0), memdump_is_colored(true), _show_obj_help(show_obj_help)
  {}

  static Mword cols()
  {
    // we show the low 8 bytes of the address
    return Jdb_screen::cols(8, sizeof(Mword)*2+1) - 1;
  }

  static Mword bytes_per_line()
  { return cols() * sizeof(Mword); }


  void init(Address ksp, Jdb_entry_frame *_ef, bool _is_current);
  void print_value(Jdb_tcb_ptr const &p, bool highl = false);
  bool handle_key(int keycode, bool *redraw);
  void edit_stack(bool *redraw);

private:
  int _show_obj_help;

  unsigned posx() const
  { return addx * (Jdb_screen::Mword_size_bmode + 1) + 9; }

  unsigned posy() const
  { return addy + start_y; }

};

class Jdb_tcb : public Jdb_module, public Jdb_kobject_handler
{
  static Kobject *threadid;
  static Address address;
  static char    first_char;
public:
  static char    auto_tcb;

private:
  static void print_return_frame_regs(Jdb_tcb_ptr const &current, Mword ksp);
  static void print_entry_frame_regs(Thread *t);
  static void info_thread_state(Thread *t);

  static Jdb_disasm_view _disasm_view;
  static Jdb_stack_view  _stack_view;

  static bool handle_obj_key(int keycode, Mword addr);
  static char *vcpu_state_str(Mword state, char *s, int len);

public:
  Jdb_tcb();

  static Action_code show(Thread *t, int level, bool dump_only);

  Action_code action(int cmd, void *&args, char const *&fmt, int &next_char) override;
  Cmd const *cmds() const override;
  int num_cmds() const override;

  Kobject_common *follow_link(Kobject_common *o) override;
  bool show_kobject(Kobject_common *o, int level) override;
  static bool is_current(Thread *t);
  void show_kobject_short(String_buffer *buf, Kobject_common *o, bool) override;

  Kobject *parent(Kobject_common *o) override
  {
    Thread *t = cxx::dyn_cast<Thread*>(o);
    if (!t)
      return nullptr;

    return static_cast<Task*>(t->space());
  }

  static void print_kobject(Thread *t, Cap_index capidx);

  static void print_thread_uid_raw(Thread *t)
  {
    printf(" <%p> ", t);
  }

  static void print_kobject(Cap_index n)
  {
    printf("[C:%4lx]       ", cxx::int_value<Cap_index>(n));
  }

  static void print_kobject(Kobject *o)
  {
    printf("D:%4lx         ", o ? o->dbg_info()->dbg_id() : 0);
  }

};
