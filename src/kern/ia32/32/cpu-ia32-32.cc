#include "cpu.h"
#include "entry-ia32.h"

extern "C" void entry_vec08_dbf();
extern "C" Address dbf_stack_top;

void FIASCO_INIT_CPU
Cpu_ia32_bits::init_tss_dbf(Address tss_dbf_mem, Address kdir)
{
  tss_dbf = reinterpret_cast<Tss*>(tss_dbf_mem);

  gdt->set_entry_tss(Gdt::gdt_tss_dbf / 8, tss_dbf_mem, sizeof(Tss) - 1);

  tss_dbf->_cs     = Gdt::gdt_code_kernel;
  tss_dbf->_ss     = Gdt::gdt_data_kernel;
  tss_dbf->_ds     = Gdt::gdt_data_kernel;
  tss_dbf->_es     = Gdt::gdt_data_kernel;
  tss_dbf->_fs     = Gdt::gdt_data_kernel;
  tss_dbf->_gs     = Gdt::gdt_data_kernel;
  tss_dbf->_eip    = (Address)entry_vec08_dbf;
  tss_dbf->_esp    = (Address)&dbf_stack_top;
  tss_dbf->_ldt    = 0;
  tss_dbf->_eflags = 0x00000082;
  tss_dbf->_cr3    = kdir;
  tss_dbf->_io_bit_map_offset = 0x8000;
}


void FIASCO_INIT_CPU
Cpu_ia32_bits::init_tss(Address tss_mem, size_t tss_size)
{
  tss = reinterpret_cast<Tss*>(tss_mem);

  gdt->set_entry_tss(Gdt::gdt_tss / 8, tss_mem, tss_size);

  tss->set_ss0(Gdt::gdt_data_kernel);
  assert(Mem_layout::Io_bitmap - tss_mem
         < (1 << (sizeof(tss->_io_bit_map_offset) * 8)));
  tss->_io_bit_map_offset = Mem_layout::Io_bitmap - tss_mem;
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
                    Gdt_entry::Kernel, Gdt_entry::Code_undef,
                    Gdt_entry::Size_32);
  gdt->set_entry_4k(Gdt::gdt_data_kernel / 8, 0, 0xffffffff,
                    Gdt_entry::Accessed, Gdt_entry::Data_write,
                    Gdt_entry::Kernel, Gdt_entry::Code_undef,
                    Gdt_entry::Size_32);
  gdt->set_entry_4k(Gdt::gdt_code_user / 8, 0, user_max,
                    Gdt_entry::Accessed, Gdt_entry::Code_read,
                    Gdt_entry::User, Gdt_entry::Code_undef,
                    Gdt_entry::Size_32);
  gdt->set_entry_4k(Gdt::gdt_data_user / 8, 0, user_max,
                    Gdt_entry::Accessed, Gdt_entry::Data_write,
                    Gdt_entry::User, Gdt_entry::Code_undef,
                    Gdt_entry::Size_32);
}

void
Cpu_ia32_bits::init_sysenter()
{
  if (sysenter())
    {
      _sysenter_eip = (Mword)entry_sys_fast_ipc_c;
      setup_sysenter();
    }
}
