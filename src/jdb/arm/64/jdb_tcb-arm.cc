
#include <jdb_tcb.h>

#include <config.h>
#include <jdb.h>

static void print_gp_regs(Mword const *r)
{
  printf(" x0 %016lx %016lx\n x2 %016lx %016lx\n", r[0], r[1], r[2], r[3]);

  unsigned cols = min<unsigned>(Jdb_screen::cols(4, 17) - 1, 6);
  unsigned rows = min<unsigned>(max<unsigned>(Jdb_screen::height(), 19) - 19, 4);

  for (unsigned i = 0, j = 0; i < 28; ++i)
    {
      if ((i % cols) == 0)
        {
          if (++j > rows)
            break;
          printf(i > 5 ? "x%u " : " x%u ", i + 4);
        }

      printf("%016lx%s", r[i + 4], ((i % cols) == (cols - 1)) ? "\n" : " ");
    }
}

void
Jdb_tcb::print_entry_frame_regs(Thread *t)
{
  Jdb_entry_frame *ef = Jdb::get_entry_frame(t->get_current_cpu());
  bool user = ef->from_user();

  printf("Regs (before debug entry from %s mode):\n",
         user ? "user" : "kernel");

  print_gp_regs(&ef->r[0]);

  printf("psr=%016lx tpidr: urw=%016lx uro=%016lx\n"
         " pc=%s%016lx\033[m %csp=%016lx x30=%016lx\n",
         ef->psr, t->_cpu_state.tpidrurw(), t->_cpu_state.tpidruro(), Jdb::esc_iret,
         ef->ip(), user ? 'u' : 'k', user ? ef->sp() : (Mword)(ef + 1),
         ef->r[30]);
}

void
Jdb_tcb::info_thread_state(Thread *t)
{
  Jdb_tcb_ptr current((Address)t->get_kernel_sp());

  printf("PC=%s%016lx\033[m USP=%016lx\n",
         Jdb::esc_emph, current.top_value(-2),  current.top_value(-3));

  print_gp_regs(current.top_value_ptr(-37));
}

void
Jdb_tcb::print_return_frame_regs(Jdb_tcb_ptr const &, Address)
{}

bool
Jdb_stack_view::edit_registers()
{ return false; }

