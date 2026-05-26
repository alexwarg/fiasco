
#include <jdb_arch.h>
#include <jdb.h>
#include <jdb_types.h>

#include "globals.h"
#include "kmem_alloc.h"
#include "space.h"
#include "mem_layout.h"
#include "mem_unit.h"
#include "static_init.h"
#include <globalconfig.h>
#include <kernel_console.h>
#include <push_console.h>

STATIC_INITIALIZE_P(Jdb, JDB_INIT_PRIO);

// disable interrupts before entering the kernel debugger
void
Jdb::save_disable_irqs(Cpu_number)
{}

// restore interrupts after leaving the kernel debugger
void
Jdb::restore_irqs(Cpu_number cpu)
{
#ifdef CONFIG_MP
  Ipi::atomic_reset(cpu, Ipi::Debug);
#else
  (void)cpu;
#endif
}

void
Jdb::enter_trap_handler(Cpu_number)
{}

void
Jdb::leave_trap_handler(Cpu_number)
{}

bool
Jdb::handle_conditional_breakpoint(Cpu_number, Jdb_entry_frame *)
{ return false; }

void
Jdb::handle_nested_trap(Jdb_entry_frame *e)
{
  printf("Trap in JDB: IP:%08lx Cause=%08lx Status=%08lx\n",
         e->ip(), e->cause, e->status);
}

bool
Jdb::handle_debug_traps(Cpu_number cpu)
{
  Jdb_entry_frame *ef = Jdb::entry_frame.cpu(cpu);
  error_buffer.cpu(cpu).clear();

  if (ef->debug_ipi())
    error_buffer.cpu(cpu).printf("IPI ENTRY");
  else if (ef->debug_entry_kernel_str())
    error_buffer.cpu(cpu).printf("%s", ef->text());
  else if (ef->debug_entry_user_str())
    error_buffer.cpu(cpu).printf("user: \"%.*s\"", ef->textlen(), ef->text());
  else
    error_buffer.cpu(cpu).printf("ENTRY");

  return true;
}

bool
Jdb::handle_user_request(Cpu_number cpu)
{
  Jdb_entry_frame *ef = Jdb::entry_frame.cpu(cpu);

  if (ef->debug_ipi())
    return cpu != Cpu_number::boot_cpu();

  if (ef->debug_entry_kernel_sequence())
    return execute_command_ni(ef->text(), ef->textlen());

  return false;
}


FIASCO_INIT void
Jdb::init()
{
  Thread::nested_trap_handler = (Trap_state::Handler)enter_jdb;
  Kconsole::console()->register_console(push_cons());
}


unsigned char *
Jdb::access_mem_task(Jdb_address addr, bool write)
{
  // no special need for MIPS here all returned mappings are writable
  (void) write;

  Address phys;
  if (addr.is_phys())
    phys = addr.phys();
  else if (addr.is_kmem())
    {
      if (addr.addr() >= Mem_layout::KSEG0 && addr.addr() <= Mem_layout::KSEG0e)
        return (unsigned char *)addr.virt();

      phys = addr.addr();
    }
  else
    {
      phys = addr.space()->virt_to_phys_s0(addr.virt());

      if (phys == (Address)-1)
        return 0;
    }

  // physical memory accessible via unmapped KSEG0
  if (phys <= Mem_layout::KSEG0e - Mem_layout::KSEG0)
    return (unsigned char *)(phys + Mem_layout::KSEG0);

  Mword old_hi = Mem_unit::entry_hi();

  Address pbits = 21;
  Address pmask = (1UL << pbits) - 1;
  Address phys_pfn = phys & ~pmask;
  Address phys_ofs = phys & pmask;

  Address map_window = 0xe0000000;
  Mem_unit::index_reg(0);
  Mem_unit::set_vz_guest_rid(Mem_unit::vz_guest_ctl1(), 0);
  Mem_unit::entry_hi(map_window);
  Mem_unit::page_mask(pmask);
  Mword e = Tlb_entry::Valid | Tlb_entry::cached | Tlb_entry::Write
            | Tlb_entry::Global;
  Mem_unit::entry_lo0(e | (phys_pfn >> 6));
  Mem_unit::entry_lo1(e | (phys_pfn >> 6) | (1UL << (pbits - 1)));

  Mips::ehb();
  Mips::tlbwi();
  Mips::ehb();

  Mem_unit::entry_hi(old_hi);
  Mips::ehb();

  // FIXME: temp mapping for the physical memory needed
  return reinterpret_cast<unsigned char *>(map_window | phys_ofs);
}

void
Jdb::write_tsc_s(String_buffer *buf, Signed64 tsc, bool sign)
{
  if (sign && tsc != 0)
    buf->printf("%+lld c", tsc);
  else
    buf->printf("%lld c", tsc);
}

void
Jdb::write_tsc(String_buffer *buf, Signed64 tsc, bool sign)
{
  write_tsc_s(buf, tsc, sign);
}


//----------------------------------------------------------------------------
#ifdef CONFIG_MP

#include <cstdio>

static
void
Jdb::send_nmi(Cpu_number cpu)
{
  printf("send_nmi to %d not implemented\n",
         cxx::int_value<Cpu_number>(cpu));
}

#endif
