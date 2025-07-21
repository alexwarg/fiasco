
#include <cstdio>
#include <cstring>
#include <minmax.h>
#include "simpleio.h"

#include "config.h"
#include "cpu.h"
#include "gdt.h"
#include "idt.h"
#include "perf_cnt.h"
#include "space.h"
#include "tss.h"
#include <x86desc_dbg.h>
#include <jdb.h>
#include <jdb_core.h>
#include <jdb_kern_info.h>
#include <kernel_console.h>


class Jdb_kern_info_idt : public Jdb_kern_info_module
{
public:
  Jdb_kern_info_idt()
    : Jdb_kern_info_module('I', "Interrupt Descriptor Table (IDT)")
  {
    Jdb_kern_info::register_subcmd(this);
  }

  void show() override
  {
    Pseudo_descriptor idt_pseudo;
    unsigned line = 0;

    Idt::get(&idt_pseudo);
    unsigned idt_max_bytes = idt_pseudo.limit() + 1;
    unsigned idt_max_entries = idt_max_bytes / sizeof(Idt_entry);

    printf("IDT base=" L4_PTR_FMT "  limit=%04x (%d entries)\n",
           idt_pseudo.base(), idt_max_bytes, idt_max_entries);
    if (!Jdb_core::new_line(line))
      return;

    Idt_entry *ie = reinterpret_cast<Idt_entry*>(idt_pseudo.base());
    // On VM exit, IDTR (and GDTR) limit are each set to 0xffff. We don't bother
    // because IDT entries beyond 256 are ignored. Show only up to 256 entries.
    for (unsigned i = 0; i < min(Idt::_idt_max, idt_max_entries); ++i)
      {
        printf("%3x: ",i);
        Dbg::desc_show(ie[i]);
        if (!Jdb_core::new_line(line))
          return;
      }
  }
};

static Jdb_kern_info_idt k_I INIT_PRIORITY(JDB_MODULE_INIT_PRIO + 1);


class Jdb_kern_info_test_tsc_scaler : public Jdb_kern_info_module
{
public:
  Jdb_kern_info_test_tsc_scaler()
    : Jdb_kern_info_module('T', "Test TSC scaler")
  {
    Jdb_kern_info::register_subcmd(this);
  }

  void show() override
  {
    while (Kconsole::console()->getchar(false) == -1)
      {
        Unsigned64 t;
        t = Cpu::boot_cpu()->ns_to_tsc(Cpu::boot_cpu()->tsc_to_ns(Cpu::rdtsc()));
        printf("Diff (press any key to stop): %llu\n", Cpu::rdtsc() - t);
      }
  }
};

static Jdb_kern_info_test_tsc_scaler k_tts INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);


#include <io.h>
#include <pc_i8259.h>

class Jdb_kern_info_pic_state : public Jdb_kern_info_module
{
public:
  Jdb_kern_info_pic_state()
    : Jdb_kern_info_module('p', "PIC ports")
  {
    Jdb_kern_info::register_subcmd(this);
  }

  void show() override
  {
    using I8259 = Pc_i8259;
    Pc_i8259 i8259;

    int i;
    static char const hex[] = "0123456789ABCDEF";

    // show important I/O ports
    i8259.write_icw_m(I8259::OCW_TEMPLATE | I8259::READ_NEXT_RD | I8259::READ_IS_ONRD);
    i8259.iodelay();
    unsigned in_service = i8259.read_icw_m();
    i8259.write_icw_m(I8259::OCW_TEMPLATE | I8259::READ_NEXT_RD | I8259::READ_IR_ONRD);
    i8259.iodelay();
    unsigned requested = i8259.read_icw_m();
    unsigned mask = Jdb::pic_status & 0x0ff;
    printf("master PIC: in service:");
    for (i=7; i>=0; i--)
      putchar((in_service & (1<<i)) ? hex[i] : '-');
    printf(", request:");
    for (i=7; i>=0; i--)
      putchar((requested & (1<<i)) ? hex[i] : '-');
    printf(", mask:");
    for (i=7; i>=0; i--)
      putchar((mask & (1<<i)) ? hex[i] : '-');
    putchar('\n');

    i8259.write_icw_s(I8259::OCW_TEMPLATE | I8259::READ_NEXT_RD | I8259::READ_IS_ONRD);
    i8259.iodelay();
    in_service = i8259.read_icw_s();
    i8259.write_icw_s(I8259::OCW_TEMPLATE | I8259::READ_NEXT_RD | I8259::READ_IR_ONRD);
    i8259.iodelay();
    requested = i8259.read_icw_s();
    mask = Jdb::pic_status >> 8;
    printf(" slave PIC: in service:");
    for (i=7; i>=0; i--)
      putchar((in_service & (1<<i)) ? hex[i+8] : '-');
    printf(", request:");
    for (i=7; i>=0; i--)
      putchar((requested & (1<<i)) ? hex[i+8] : '-');
    printf(", mask:");
    for (i=7; i>=0; i--)
      putchar((mask & (1<<i)) ? hex[i+8] : '-');
    putchar('\n');
  }
};

static Jdb_kern_info_pic_state k_p INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);



class Jdb_kern_info_misc : public Jdb_kern_info_module
{
public:
  Jdb_kern_info_misc()
    : Jdb_kern_info_module('i', "Miscellaneous info")
  {
    Jdb_kern_info::register_subcmd(this);
  }

  void show() override
  {
    Cpu_time clock = Jdb::system_clock_on_enter();
    printf ("clck: %08x.%08x\n", static_cast<unsigned>(clock >> 32), static_cast<unsigned>(clock));

    show_pdir();

    Pseudo_descriptor gdt_pseudo, idt_pseudo;
    Gdt::get (&gdt_pseudo);
    Idt::get (&idt_pseudo);
    printf ("idt : base=" L4_PTR_FMT "  limit=%04x\n"
            "gdt : base=" L4_PTR_FMT "  limit=%04x\n",
            idt_pseudo.base(), static_cast<unsigned>(idt_pseudo.limit()+1)/8,
            gdt_pseudo.base(), static_cast<unsigned>(gdt_pseudo.limit()+1)/8);

    // print LDT
    printf("ldt : %04x", static_cast<unsigned>(Cpu::get_ldt()));
    if (Cpu::get_ldt() != 0)
      {
        Gdt_entry *e = Cpu::boot_cpu()->get_gdt()->entries() + (Cpu::boot_cpu()->get_ldt() >> 3);
        printf(": " L4_PTR_FMT "-" L4_PTR_FMT,
            e->base(), e->base()+ e->size());
      }

    // print TSS
    printf("\n"
           "tr  : %04x", static_cast<unsigned>(Cpu::boot_cpu()->get_tr()));
    if(Cpu::get_tr() != 0)
      {
        Gdt_entry *e = Cpu::boot_cpu()->get_gdt()->entries() + (Cpu::boot_cpu()->get_tr() >> 3);
        printf(": " L4_PTR_FMT "-" L4_PTR_FMT ", iobitmap at " L4_PTR_FMT,
               e->base(), e->base()+ e->size(),
               e->base() + (reinterpret_cast<Tss *>(e->base())->_io_bit_map_offset));
      }
    printf("\n"
           "cr0 : " L4_PTR_FMT "\n"
           "cr4 : " L4_PTR_FMT "\n",
           Cpu::get_cr0(), Cpu::get_cr4());
  }


private:
  void show_pdir()
  {
    Mem_space *s = Mem_space::current_mem_space(Cpu_number::boot_cpu());
    printf("%s" L4_PTR_FMT "\n",
           Jdb_screen::Root_page_table, (Address)s->dir());
  }
};

static Jdb_kern_info_misc k_i INIT_PRIORITY(JDB_MODULE_INIT_PRIO + 1);


class Jdb_kern_info_cpu : public Jdb_kern_info_module
{
public:
  Jdb_kern_info_cpu()
    : Jdb_kern_info_module('c', "CPU features")
  {
    Jdb_kern_info::register_subcmd(this);
  }

  void show() override
  {
    const char *perf_type = Perf_cnt::perf_type();
    char cpu_mhz[32];
    char time[32];
    unsigned hz;
    static char const * const scheduler_mode[]
      = { "PIT", "RTC", "APIC", "HPET" };

    cpu_mhz[0] = '\0';
    if ((hz = Cpu::boot_cpu()->frequency()))
      {
        unsigned mhz = hz / 1000000;
        hz -= mhz * 1000000;
        unsigned khz = hz / 1000;
        snprintf(cpu_mhz, sizeof(cpu_mhz), "%u.%03u MHz", mhz, khz);
      }

    char model_str[52];
    int model_str_len = Cpu::get_model_str_current_cpu(model_str);
    printf("CPU: %.*s %s\n", model_str_len, model_str, cpu_mhz);
    show_features();

    if (Cpu::boot_cpu()->tsc())
      {
        Unsigned32 hour, min, sec, ns;
        Cpu::boot_cpu()->tsc_to_s_and_ns(Cpu::rdtsc(), &sec, &ns);
        hour = sec  / 3600;
        sec -= hour * 3600;
        min  = sec  / 60;
        sec -= min  * 60;
        snprintf(time, sizeof(time), "%02u:%02u:%02u.%06u",
                 hour, min, sec, ns/1000);
      }
    else
      strcpy(time, "not available");

    printf("\nTimer interrupt source: %s (irq vector 0x%02x)"
           "\nPerformance counters: %s"
           "\nLast branch recording: %s"
           "\nDebug store to memory: %s"
           "\nTime stamp counter: %s"
           "\n",
           scheduler_mode[Config::Scheduler_mode],
           Config::scheduler_irq_vector,
           perf_type ? perf_type : "no",
           Cpu::boot_cpu()->lbr_type() != Cpu::Lbr_unsupported
              ? Cpu::boot_cpu()->lbr_type() == Cpu::Lbr_pentium_4 ? "P4" : "P6"
              : "no",
           Cpu::boot_cpu()->bts_type() != Cpu::Bts_unsupported
              ? Cpu::boot_cpu()->bts_type() == Cpu::Bts_pentium_4 ? "P4" : "Pentium-M"
              : "no",
           time
           );
  }

  void show_f_bits(unsigned features, const char *const *table,
                   unsigned first_pos, unsigned &last_pos,
                   unsigned &colon)
  {
    unsigned i, count;

    for (i = count = 0; *table != (char *)-1; i++, table++)
      if ((features & (1 << i)) && *table)
        {
          int slen = strlen(*table);
          if (last_pos+colon + slen > 78)
            {
              colon = 0;
              last_pos = first_pos;
              printf("\n%*s", (int)first_pos, "");
            }
          printf ("%s%s", colon ? ", " : "", *table);
          last_pos += slen + colon;
          colon = 2;
        }
  }

  void show_features()
  {
    static const char *const simple[] =
    {
      "fpu (fpu on chip)",
      "vme (virtual-8086 mode enhancements)",
      "de (I/O breakpoints)",
      "pse (4MB pages)",
      "tsc (rdtsc instruction)",
      "msr (rdmsr/rdwsr instructions)",
      "pae (physical address extension)",
      "mce (machine check exception #18)",
      "cx8 (cmpxchg8 instruction)",
      "apic (on-chip APIC)",
      nullptr,
      "sep (sysenter/sysexit instructions)",
      "mtrr (memory type range registers)",
      "pge (global TLBs)",
      "mca (machine check architecture)",
      "cmov (conditional move instructions)",
      "pat (page attribute table)",
      "pse36 (32-bit page size extension)",
      "psn (processor serial number)",
      "clfsh (flush cache line instruction)",
      nullptr,
      "ds (debug store to memory)",
      "acpi (thermal monitor and soft controlled clock)",
      "mmx (MMX technology)",
      "fxsr (fxsave/fxrstor instructions)",
      "sse (SSE extensions)",
      "sse2 (SSE2 extensions)",
      "ss (self snoop of own cache structures)",
      "htt (hyper-threading technology)",
      "tm (thermal monitor)",
      nullptr,
      "pbe (pending break enable)",
      (char *)(-1)
    };
    static const char *const extended[] =
    {
      "pni (prescott new instructions)",
      nullptr, nullptr,
      "monitor (monitor/mwait instructions)",
      "dscpl (CPL qualified debug store)",
      "vmx (virtual machine technology)",
      nullptr,
      "est (enhanced speedstep technology)",
      "tm2 (thermal monitor 2)",
      nullptr,
      "cid (L1 context id)",
      nullptr, nullptr,
      "cmpxchg16b",
      "xtpr (send task priority messages)",
      nullptr, nullptr,
      "pcid",
      nullptr,
      "sse41", "sse42",
      "x2apic",
      nullptr,
      "popcnt", nullptr,
      "aes", "xsave", "osxsave",
      "avx", "f16c",
      (char *)(-1)
    };
    static const char *const ext_81_ecx[] =
    {
      nullptr, nullptr, "svm (secure virtual machine)", nullptr, nullptr,
      "abm (adv bit manipulation)", "SSE4A", nullptr,
      nullptr, "OSVW (OS visible workaround)", nullptr, nullptr,
      "SKINIT", "WDT (watchdog timer support)", nullptr,
      "lwp", "fmaa", nullptr, nullptr, "nodeid", nullptr, "tbm", "topext",
      (char *)(-1)
    };
    static const char *const ext_81_edx[] =
    {
      nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr,
      "syscall (syscall/sysret instructions)",
      nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr,
      "mp (MP capable)",
      "nx (no-execute page protection)",
      nullptr,
      "mmxext (AMD extensions to MMX)",
      nullptr, nullptr,
      "fxsr_opt (FXSR optimizations)",
      "Page1GB",
      "RDTSCP",
      nullptr, // reserved
      "lm (Long mode)",
      "3dnowext (AMD 3DNow! extenstion)",
      "3dnow (3DNow! instructions)",
      (char *)(-1)
    };

    unsigned position = 5, colon = 0;
    putstr("\nCPU features:\n     ");
    show_f_bits (Cpu::boot_cpu()->features(), simple, 5, position, colon);
    show_f_bits (Cpu::boot_cpu()->ext_features(), extended, 5, position, colon);
    show_f_bits (Cpu::boot_cpu()->ext_8000_0001_ecx(), ext_81_ecx, 5, position, colon);
    show_f_bits (Cpu::boot_cpu()->ext_8000_0001_edx(), ext_81_edx, 5, position, colon);

    puts("\n\nRaw CPUID features:");
    // below we use arbitrary upper limits for basic/extended leaf
    Unsigned32 max = min(0x2fU, Cpu::cpuid_eax(0));
    for (Unsigned32 i = 0; i <= max; ++i)
      {
        Unsigned32 eax, ebx, ecx, edx;
        Cpu::cpuid(i, 0, &eax, &ebx, &ecx, &edx);
        printf("     %08xH: %08x %08x %08x %08x\n", i, eax, ebx, ecx, edx);
        if (i == 0x7) // extended features
          for (unsigned c = 1; c <= 2; ++c)
            {
              Cpu::cpuid(i, c, &eax, &ebx, &ecx, &edx);
              if (eax == 0 && ebx == 0 && ecx == 0 && edx == 0)
                break; // invalid sub-leaf
              printf("         ecx=%u: %08x %08x %08x %08x\n", c, eax, ebx, ecx, edx);
            }
        else if (i == 0xb || i == 0x1f) // extended topology
          for (unsigned c = 1; c < 7; ++c)
            {
              Cpu::cpuid(i, c, &eax, &ebx, &ecx, &edx);
              if (eax == 0 && ebx == 0)
                break; // invalid domain
              printf("         ecx=%u: %08x %08x %08x %08x\n", c, eax, ebx, ecx, edx);
            }
        if (i == max && max < 0x80000000U)
          {
            i = 0x80000000 - 1;
            max = min(0x8000001fU, Cpu::cpuid_eax(i + 1));
            putchar('\n');
          }
      }
  }
};

static Jdb_kern_info_cpu k_c INIT_PRIORITY(JDB_MODULE_INIT_PRIO + 1);


class Jdb_kern_info_gdt : public Jdb_kern_info_module
{
public:
  Jdb_kern_info_gdt()
    : Jdb_kern_info_module('g', "Global Descriptor Table (GDT)")
  {
    Jdb_kern_info::register_subcmd(this);
  }


  static void show_gdt(Cpu_number cpu)
  {
    Gdt *gdt = Cpu::cpus.cpu(cpu).get_gdt();
    unsigned entries = Gdt::gdt_max / 8;

    if (Config::Max_num_cpus > 1)
      printf("CPU%u: GDT base=" L4_PTR_FMT "  limit=%04x (%04x bytes)\n",
             cxx::int_value<Cpu_number>(cpu), reinterpret_cast<Mword>(gdt), entries,
             static_cast<unsigned>(Gdt::gdt_max));
    else
      printf("GDT base=" L4_PTR_FMT "  limit=%04x (%04x bytes)\n",
             reinterpret_cast<Mword>(gdt), entries, static_cast<unsigned>(Gdt::gdt_max));

    if (!Jdb_core::new_line(line))
      return;

    for (unsigned i = 0; i < entries; i++)
      {
        printf(" %02x: ", i * 8);
        if (i == 0)
          printf("(ignored)\n");
        else
          Dbg::desc_show((*gdt)[i]);
        if (!Jdb_core::new_line(line))
          return;
        if (i != 0 && (*gdt)[i].desc_size() == 16)
          ++i;
      }
  }

  void show() override
  {
    line = 0;
    Jdb::foreach_cpu(&show_gdt);
  }

private:
  static unsigned line;
};

static Jdb_kern_info_gdt k_g INIT_PRIORITY(JDB_MODULE_INIT_PRIO + 1);

unsigned Jdb_kern_info_gdt::line;

// ------------------------------------------------------------------------
#ifdef CONFIG_SCHED_HPET

#include "hpet.h"

class Jdb_kern_info_hpet_smm : public Jdb_kern_info_module
{
public:
  Jdb_kern_info_hpet_smm::Jdb_kern_info_hpet_smm()
    : Jdb_kern_info_module('S', "SMM loop using HPET")
  {
    Jdb_kern_info::register_subcmd(this);
  }

  void show() override
  {
    const unsigned config_spin_loops = 10000;
    const unsigned config_hist_loops = 60;
    unsigned delta = 1;
    Mword counter_good = 0;
    Mword histsum = 0;
    Mword hist_loops = config_hist_loops;

    printf("HPET SMM Check: Press key to stop.\n");
    printf("HPET SMM Check Loop testing (loops=%d)\n", config_spin_loops);

    Hpet::hpet()->dump();
    Hpet::hpet()->enable();
    while (1)
      {
        Unsigned64 x1 = Hpet::hpet()->counter_val;

        int i = config_spin_loops;
        while (i--)
          asm volatile("" : : : "memory");

        Unsigned64 diff = Hpet::hpet()->counter_val - x1;

        if (hist_loops)
          {
            histsum += diff;
            --hist_loops;

            if (hist_loops == 0)
              {
                delta = (histsum + histsum / 9) / config_hist_loops;
                printf("HPET SMM Check threshold=%dhpet-clks %lldus\n",
                       delta,
                       (delta * Hpet::hpet()->counter_clk_period()) / 1000000000ULL);
              }
          }
        else
          {
            if (diff > delta && diff < (~0UL - delta * 2))
              {
                printf("%lld  %lldus (before %ld good iterations)\n", diff,
                       (diff * Hpet::hpet()->counter_clk_period()) / 1000000000ULL,
                       counter_good);
                counter_good = 0;
                if (Kconsole::console()->getchar(false) != -1)
                  break;
              }
            else
              ++counter_good;

            if (counter_good % 30000 == 2)
              if (Kconsole::console()->getchar(false) != -1)
                break;
          }
      }
  }
};

static Jdb_kern_info_hpet_smm ki_smm INIT_PRIORITY(JDB_MODULE_INIT_PRIO + 1);

#endif
