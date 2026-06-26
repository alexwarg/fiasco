#include "cpu.h"

#include <cstdio>
#include <cstdarg>
#include "panic.h"
#include "cp0_status.h"
#include "alternatives.h"
#include "mem_layout.h"
#include "processor.h"

DEFINE_PER_CPU_P(0) Per_cpu<Cpu> Cpu::cpus(Per_cpu_data::Cpu_num);
Cpu *Cpu::_boot_cpu;
unsigned long Cpu::_ns_per_cycle;
unsigned Cpu::_tlb_size;
unsigned Cpu::_ftlb_sets;
unsigned Cpu::_ftlb_ways;
unsigned Cpu::_default_cca;
Cpu::Options Cpu::options;

void
Cpu::panic(char const *fmt, ...) const
{
  va_list list;
  va_start(list, fmt);
  printf("CPU[%d]: panic: ", cxx::int_value<Cpu_number>(id()));
  vprintf(fmt, list);
  va_end(list);
  ::panic("panic");
}

void
Cpu::require(bool cond, char const *fmt, ...) const
{
  if (cond)
    return;

  va_list list;
  va_start(list, fmt);
  printf("CPU[%d]: panic: ", cxx::int_value<Cpu_number>(id()));
  vprintf(fmt, list);
  va_end(list);
  ::panic("panic");
}

void
Cpu::pr(char const *fmt, ...) const
{
  va_list list;
  va_start(list, fmt);
  printf("CPU[%d]: ", cxx::int_value<Cpu_number>(id()));
  vprintf(fmt, list);
  va_end(list);
}

bool
Cpu::if_show_infos() const
{ return id() == Cpu_number::boot_cpu() || !boot_cpu(); }

void
Cpu::print_infos() const
{
  if (if_show_infos())
    pr("%lluMHz (%d TLBs) %s\n",
       frequency() / 1000000, tlb_size(),
       options.tlbinv() ? "TLBINV " : "");
}

void
Cpu::first_boot(bool is_boot_cpu)
{
  //AW: identify();

  auto c = Mips::Configs::read();
  if (c.r<0>().mt() != 1 && c.r<0>().mt() != 4)
    {
      char const *const tlb_type[] =
        {
          "None", "Standard TLB", "BAT", "Fixed Map",
          "Dual VTLB / FTLB", "<unk>", "<unk>", "<unk>"
        };
      panic("unsupported TLB type: %s (%u)\n",
            tlb_type[c.r<0>().mt()], static_cast<unsigned>(c.r<0>().mt()));
    }

  require(c.r<0>().vi() == 0, "virtual instruction caches not supported\n");
  require(c.r<0>().ar() > 0,  "MIPS r1 CPUs are notsupported\n");
  require(c.r<0>().m(), "CP0 Config1 register missing\n");

  require(c.r<1>().m(), "CP0 Config2 register missing\n");
  require(c.r<2>().m(), "CP0 Config3 register missing\n");

  unsigned tlb_size  = c.r<1>().mmu_size();
  unsigned ftlb_ps   = 0;
  unsigned ftlb_info = 0;

  Options opts = { 0 };

  opts.ulr() = c.r<3>().ulri();
  opts.vz()  = c.r<3>().vz();
  opts.bi()  = c.r<3>().bi();
  opts.bp()  = c.r<3>().bp();
  opts.hwpw() = c.r<3>().pw();
  opts.segctl() = c.r<3>().sc();

  if (c.r<3>().m())
    {
      if (c.r<0>().ar() == 2 || c.r<4>().mmu_ext_def() == 3)
        {
          tlb_size |= static_cast<unsigned>(c.r<4>().vtlb_sz_ext()) << 6;
          ftlb_info = c.r<4>().ftlb_info();
          ftlb_ps = c.r<4>().ftlb_page_size2();
        }
      else if (c.r<4>().mmu_ext_def() == 2)
        {
          ftlb_info = c.r<4>().ftlb_info();
          ftlb_ps = c.r<4>().ftlb_page_size1();
        }
      else if (c.r<4>().mmu_ext_def() == 1)
        tlb_size |= static_cast<unsigned>(c.r<4>().mmu_sz_ext()) << 6;

      if (ftlb_info)
        {
          unsigned ps = 0;
          switch (static_cast<Mword>(Config::PAGE_SIZE))
            {
            case 0x1000: // try to enable 4 KiB pages in FTLB
              ps = 1;
              break;
            case 0x4000: // try to enable 16 KiB pages in FTLB
              ps = 2;
              break;
            default:
              panic("FTLB: page size (0x%x) not supported with FTLB\n",
                    Config::PAGE_SIZE);
              break;
            }

          auto c4 = c.r<4>();
          c4.ftlb_page_size2() = ps;
          Mips::mtc0_32(c4._v, Mips::Cp0_config_4);
          Mips::ehb();
          c4._v = Mips::mfc0_32(Mips::Cp0_config_4);
          require(c4.ftlb_page_size2() == ps,
                  "FTLB: page size (0x%x) not supported in HW\n",
                  Config::PAGE_SIZE);
          c.r<4>()._v = c4._v;
          ftlb_ps = ps;
          require(c.r<4>().ie() > 1, "FTLB: missing TLBINV support\n");
          opts.ftlb() = true;
          opts.ftlbinv() = (c.r<4>().ie() == 3);
        }

      if (c.r<4>().ie() > 1)
        opts.tlbinv() = true;
    }

  if (is_boot_cpu)
    {
      _boot_cpu = this;
      set_present(1);
      set_online(1);
      _ns_per_cycle = 1000000000 / frequency();
      _tlb_size = tlb_size + 1;
      options = opts;
      pr("TLB entries: %u\n", tlb_size + 1);
      if (opts.ftlb())
        {
          _ftlb_sets = 1U << (ftlb_info & 0xf);
          _ftlb_ways = (ftlb_info >> 4) + 2;
          pr("TLB: FTLB: page_size=%u sets=%u ways=%u\n",
             ftlb_ps, _ftlb_sets, _ftlb_ways);
        }

      _default_cca = c.r<0>().k0();

      // patch instruction alternatives for detected options
      Alternative_insn::handle_alternatives(opts._o);
    }
  else
    {
      require(_tlb_size == tlb_size + 1, "TLB size mismatch: %d <> %d\n",
              _tlb_size, tlb_size + 1);

      require(opts._o == options._o, "conflicting CPU options: %lx <> %lx\n",
              options._o, opts._o);
    }

  _phys_id = Proc::cpu_id();
  print_infos();
}

void
Cpu::init(Cpu_number cpu, bool resume, bool is_boot_cpu)
{
  Unsigned32 prid = Mips::mfc0_32(Mips::Cp0_proc_id);
  for (Cpu_type *t = _types; t->hooks; ++t)
    {
      if ((prid & t->id_mask) == t->id)
        t->hooks->init(cpu, resume, prid);
    }

  if (!resume)
    first_boot(is_boot_cpu);

  if (options.segctl())
    {
      // setup segments for our purposes
      Mword cca = _default_cca;
      // kseg3 is currently kernel only mapped (used by JDB)
      // kseg kernel / user mapped
      Mips::mtc0(0x00300010, Mips::Cp0_seg_ctl_0);
      // kseg1 kernel unmapped uncached, user mapped
      // kseg0 kernel unmapped cached, user mapped
      // xam == 0 (UK)
      Mips::mtc0(0x00400042 | (cca << 16), Mips::Cp0_seg_ctl_1);

      // cached and mapped in user mode, xr == 0 (xphys disabled)
      Mips::mtc0(0x04300030 | (cca << 16) | cca, Mips::Cp0_seg_ctl_2);
      Mips::ehb();

      // now we should have a user address space up to 0xe0000000
    }

  Mips::mtc0(Mem_layout::Exception_base, Mips::Cp0_ebase);

  Cp0_status::write(Cp0_status::ST_DEFAULT);
  Mips::mtc0_32(0, Mips::Cp0_cause);
  Mips::ehb();

  Mword hwrena = 0xf;
  if (options.ulr())
    hwrena |= 0x20000000;

  /* Set the HW Enable register to allow rdhwr access from UM */
  Mips::mtc0_32(hwrena, Mips::Cp0_hw_rena);
}
