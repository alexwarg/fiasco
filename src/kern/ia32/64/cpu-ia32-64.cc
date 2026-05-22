#include "cpu.h"

#include "mem_layout.h"
#include "syscall_entry.h"
#include "tss.h"

extern "C" void entry_sys_fast_ipc_c();
extern "C" Address dbf_stack_top;

#ifndef CONFIG_KERNEL_ISOLATION
Per_cpu_array<Syscall_entry_data> Cpu_ia32_bits::_syscall_entry_data;
#endif // CONFIG_KERNEL_ISOLATION

void FIASCO_INIT_CPU
Cpu_ia32_bits::init_tss(Address tss_mem, size_t tss_size)
{
  tss = reinterpret_cast<Tss*>(tss_mem);

  gdt->set_entry_tss(Gdt::gdt_tss / 8, tss_mem, tss_size);

  // XXX setup pointer for clean double fault stack
  tss->_ist1 = (Address)&dbf_stack_top;
  assert(Mem_layout::Io_bitmap - tss_mem
         < (1 << (sizeof(tss->_io_bit_map_offset) * 8)));
  tss->_io_bit_map_offset = Mem_layout::Io_bitmap - tss_mem;
  init_sysenter();
}

void FIASCO_INIT_CPU
Cpu_ia32_bits::init_gdt(Address gdt_mem, Address user_max)
{
  gdt = new ((void *)gdt_mem) Gdt();

  // make sure kernel cs/ds and user cs/ds are placed in the same
  // cache line, respectively; pre-set all "accessed" flags so that
  // the CPU doesn't need to do this later

  gdt->set_entry_4k(Gdt::gdt_code_kernel / 8, 0, 0xffffffff,
                    Gdt_entry::Accessed, Gdt_entry::Code_read,
                    Gdt_entry::Kernel, Gdt_entry::Code_64bit,
                    Gdt_entry::Size_undef);
  gdt->set_entry_4k(Gdt::gdt_data_kernel / 8, 0, 0xffffffff,
                    Gdt_entry::Accessed, Gdt_entry::Data_write,
                    Gdt_entry::Kernel, Gdt_entry::Code_undef,
                    Gdt_entry::Size_32);
  gdt->set_entry_4k(Gdt::gdt_code_user / 8, 0, user_max,
                    Gdt_entry::Accessed, Gdt_entry::Code_read,
                    Gdt_entry::User, Gdt_entry::Code_64bit,
                    Gdt_entry::Size_undef);
  gdt->set_entry_4k(Gdt::gdt_data_user / 8, 0, user_max,
                    Gdt_entry::Accessed, Gdt_entry::Data_write,
                    Gdt_entry::User, Gdt_entry::Code_undef,
                    Gdt_entry::Size_32);
  gdt->set_entry_4k(Gdt::gdt_code_user32 / 8, 0, user_max,
                    Gdt_entry::Accessed, Gdt_entry::Code_read,
                    Gdt_entry::User, Gdt_entry::Code_compat,
                    Gdt_entry::Size_32);
}



