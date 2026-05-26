
#include <jdb_tcb.h>

#include <config.h>
#include <jdb.h>

#include <globalconfig.h>

void
Jdb_tcb::print_return_frame_regs(Jdb_tcb_ptr const &, Address)
{}

bool
Jdb_stack_view::edit_registers()
{ return true; }


#ifdef CONFIG_BIT32

void
Jdb_tcb::info_thread_state(Thread *t)
{
  Mem_space *s = t->mem_space();

  Jdb_tcb_ptr current((Address)t->get_kernel_sp());

  printf("\n"
         "Cause=%08lx Status=%08lx Epc=%08lx\n"
         "BadVaddr=%08lx Asid=%lx Hi=%lx Lo=%lx\n",
         current.top_value(-3), current.top_value(-2), current.top_value(-1),
         current.top_value(-4), s->c_asid(), current.top_value(-5),
         current.top_value(-6));

  for (unsigned i = 0; i < 32; ++i)
    {
      if ((i & 7) == 0)
        printf("[%2u] ", i);

      printf("%08lx%s", current.top_value(-38 + i),
             ((i & 7) == 7) ? "\n" : (((i & 7) == 3) ? "  " : " "));
    }
}

void Jdb_tcb::print_entry_frame_regs(Thread *t)
{
  Jdb_entry_frame *ef = Jdb::get_entry_frame(t->get_current_cpu());
  int from_user       = ef->from_user();
  Mem_space *s = t->mem_space();

  printf("\n\n\nRegisters (before debug entry from %s mode):\n"
         " [0] %08lx %08lx %08lx %08lx  %08lx %08lx %08lx %08lx\n"
         " [8] %08lx %08lx %08lx %08lx  %08lx %08lx %08lx %08lx\n"
         "[16] %08lx %08lx %08lx %08lx  %08lx %08lx %08lx %08lx\n"
         "[24] %08lx %08lx %08lx %08lx  %08lx %08lx %08lx %08lx\n"
         "Hi=%08lx Lo=%08lx Pf=%08lx Ca=%08lx St=%08lx Epc=%08lx Asid=%lx\n",
         from_user ? "user" : "kernel",
         ef->r[0],  ef->r[1], ef->r[2], ef->r[3], ef->r[4],
         ef->r[5],  ef->r[6], ef->r[7], ef->r[8], ef->r[9],
         ef->r[10], ef->r[11], ef->r[12], ef->r[13], ef->r[14],
         ef->r[15], ef->r[16], ef->r[17], ef->r[18], ef->r[19],
         ef->r[20], ef->r[21], ef->r[22], ef->r[23], ef->r[24],
         ef->r[25], ef->r[26], ef->r[27], ef->r[28], ef->r[29],
         ef->r[30], ef->r[31], ef->hi, ef->lo, ef->bad_v_addr,
         ef->cause, ef->status, ef->epc, s->c_asid());
}

#endif
#ifdef CONFIG_BIT64

void
Jdb_tcb::info_thread_state(Thread *t)
{
  Mem_space *s = t->mem_space();

  Jdb_tcb_ptr current((Address)t->get_kernel_sp());

  printf("Ca=%08lx St=%08lx Epc=%08lx\n"
         "BadVA=%08lx Asid=%hx Hi=%lx Lo=%lx\n",
         current.top_value(-3), current.top_value(-2), current.top_value(-1),
         current.top_value(-4), s->c_asid(), current.top_value(-5),
         current.top_value(-6));

  unsigned cols = Jdb_screen::cols(5, 17) - 1;
  if (cols > 6)
    cols = 6;
  for (unsigned i = 0, j = 0; i < 32; ++i)
    {
      if ((i % cols) == 0)
        {
          if (++j > 6)
            break;
          printf("  [%2u]  ", i);
        }

      printf("%016lx%s", current.top_value(-38 + i),
             ((i % cols) == (cols-1)) ? "\n" : " ");
    }
}

void Jdb_tcb::print_entry_frame_regs(Thread *t)
{
  Jdb_entry_frame *ef = Jdb::get_entry_frame(t->get_current_cpu());
  int from_user       = ef->from_user();
  Mem_space *s = t->mem_space();

  printf("Regs (before debug entry from %s mode):\n"
         "[0]  %016lx  %016lx\n"
         "[2]  %016lx  %016lx\n"
         "[4]  %016lx  %016lx  %016lx  %016lx\n"
         "[8]  %016lx  %016lx  %016lx  %016lx\n"
         "Hi=%08lx Lo=%08lx Pf=%08lx Ca=%08lx St=%08lx Epc=%08lx Asid=%hx\n",
         from_user ? "user" : "kernel",
         ef->r[0],  ef->r[1], ef->r[2], ef->r[3], ef->r[4],
         ef->r[5],  ef->r[6], ef->r[7], ef->r[8], ef->r[9],
         ef->r[10], ef->r[11], ef->hi, ef->lo, ef->bad_v_addr,
         ef->cause, ef->status, ef->epc, s->c_asid());
}

#endif
