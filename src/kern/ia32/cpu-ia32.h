#pragma once

#include "cpu.h"
#include "asm.h"
#include "types.h"
#include "initcalls.h"
#include "regdefs.h"
#include "per_cpu_data.h"
#include "l4_types.h"
#include "gdt.h"
#include <cpu_generic.h>

#define FIASCO_IA32_LOAD_SEG_SAFE(seg, val) \
  asm volatile ("mov %0, %%" #seg : : "rm"(val))

#define FIASCO_IA32_LOAD_SEG(seg, val) \
  asm volatile (                              \
    "1: mov %0, %%" #seg "\n\t"               \
    ".pushsection \".fixup.%=\", \"ax?\"\n\t" \
    "2: movw  $0, %0                    \n\t" \
    "   jmp 1b                          \n\t" \
    ".popsection                        \n\t" \
    ASM_KEX(1b, 2b)                           \
    : : "rm" (val))

class Gdt;
class Tss;

class Cpu_ia32 : public Cpu_generic
{
  MEMBER_OFFSET();
  friend class Kip_test;

public:

  enum Vendor
  {
    Vendor_unknown = 0,
    Vendor_intel,
    Vendor_amd,
    Vendor_cyrix,
    Vendor_via,
    Vendor_umc,
    Vendor_nexgen,
    Vendor_rise,
    Vendor_transmeta,
    Vendor_sis,
    Vendor_nsc
  };

  enum CacheTLB
  {
    Cache_unknown = 0,
    Cache_l1_data,
    Cache_l1_inst,
    Cache_l1_trace,
    Cache_l2,
    Cache_l3,
    Tlb_data_4k,
    Tlb_inst_4k,
    Tlb_data_4M,
    Tlb_inst_4M,
    Tlb_data_4k_4M,
    Tlb_inst_4k_4M,
    Tlb_data_2M_4M,
  };

  enum
  {
    Ldt_entry_size = 8,
  };

  enum Local_features
  {
    Lf_rdpmc            = 1U << 0,  // supports RDPMC instruction
    Lf_rdpmc32          = 1U << 1,  // supports RDPMC32 instruction
    Lf_tsc_invariant    = 1U << 2,  // TSC runs at constant rate and does not
                                    // stop in any ACPI state
  };

  enum Lbr
  {
    Lbr_uninitialized = 0,
    Lbr_unsupported,
    Lbr_pentium_6,
    Lbr_pentium_4,
    Lbr_pentium_4_ext,
  };

  enum Bts
  {
    Bts_uninitialized = 0,
    Bts_unsupported,
    Bts_pentium_m,
    Bts_pentium_4,
  };

  enum Xstate : Unsigned64
  {
    Xstate_fp           = 1 << 0,
    Xstate_sse          = 1 << 1,
    Xstate_avx          = 1 << 2,
    Xstate_avx512       = 0x7 << 5,
    Xstate_defined_bits = Xstate_fp | Xstate_sse | Xstate_avx | Xstate_avx512,
  };

  bool tsc_frequency_accurate()
  {
    return tsc_frequency_from_cpuid_15h(true);
  }

  void enable_ldt(Address addr, int size)
  {
    if (!size)
      {
        get_gdt()->clear_entry(Gdt::gdt_ldt / 8);
        set_ldt(0);
      }
    else
      {
        get_gdt()->set_entry_ldt(Gdt::gdt_ldt / 8, addr, size - 1);
        set_ldt(Gdt::gdt_ldt);
      }
  }


  void disable(Cpu_number cpu, char const *reason);

  char const *model_str() const { return _model_str; }
  Vendor vendor() const { return _vendor; }

  unsigned family() const
  { return (_version >> 8 & 0xf) + (_version >> 20 & 0xff); }

  char const *vendor_str() const
  { return _vendor == Vendor_unknown ? "Unknown" : vendor_ident[_vendor]; }

  unsigned model() const
  { return (_version >> 4 & 0xf) + (_version >> 12 & 0xf0); }

  unsigned stepping() const { return _version & 0xF; }
  unsigned type() const { return (_version >> 12) & 0x3; }
  Unsigned64 frequency() const { return _frequency; }
  unsigned brand() const { return _brand & 0xFF; }
  unsigned features() const { return _features; }
  unsigned ext_features() const { return _ext_features; }
  bool has_monitor_mwait() const { return _ext_features & FEATX_MONITOR; }
  bool has_monitor_mwait_irq() const { return _monitor_mwait_ecx & 3; }
  bool has_pcid() const { return _ext_features & FEATX_PCID; }

  bool __attribute__((const)) has_smep() const
  { return _ext_07_ebx & FEATX_SMEP; }

  bool __attribute__((const)) has_invpcid() const
  { return _ext_07_ebx & FEATX_INVPCID; }

  bool __attribute__((const)) has_l1d_flush() const
  { return (_ext_07_edx & FEATX_L1D_FLUSH); }

  bool __attribute__((const)) has_arch_capabilities() const
  { return (_ext_07_edx & FEATX_IA32_ARCH_CAPABILITIES); }

  bool __attribute ((const)) skip_l1dfl_vmentry() const
  { return (_arch_capabilities & (1UL << 3)); }

  unsigned ext_8000_0001_ecx() const { return _ext_8000_0001_ecx; }
  unsigned ext_8000_0001_edx() const { return _ext_8000_0001_edx; }
  unsigned local_features() const { return _local_features; }
  bool superpages() const { return features() & FEAT_PSE; }
  bool tsc() const { return features() & FEAT_TSC; }
  bool sysenter() const { return features() & FEAT_SEP; }
  bool syscall() const { return ext_8000_0001_edx() & FEATA_SYSCALL; }
  unsigned virt_bits() const { return _virt_bits; }
  unsigned phys_bits() const { return _phys_bits; }
  Unsigned32 get_scaler_tsc_to_ns() const { return scaler_tsc_to_ns; }
  Unsigned32 get_scaler_tsc_to_us() const { return scaler_tsc_to_us; }
  Unsigned32 get_scaler_ns_to_tsc() const { return scaler_ns_to_tsc; }

  Address volatile &kernel_sp() const;

  void update_features_info();

  bool has_xsave() const { return ext_features() & FEATX_XSAVE; }

  Lbr lbr_type() const { return _lbr; }
  Bts bts_type() const { return _bts; }
  bool lbr_status() const { return lbr_active; }
  bool bts_status() const { return bts_active; }
  bool btf_status() const { return btf_active; }

  Gdt* get_gdt() const { return gdt; }
  Tss* get_tss() const { return tss; }
  void set_gdt() const
  {
    Pseudo_descriptor desc((Address)gdt, Gdt::gdt_max-1);
    Gdt::set (&desc);
  }

  static void set_tss() { set_tr(Gdt::gdt_tss); }

  /// Return the CPU's microcode revision
  static Unsigned32 ucode_revision()
  {
    Unsigned32 a, b, c, d;
    wrmsr(0, 0x8b); // IA32_BIOS_SIGN_ID
    cpuid(1, &a, &b, &c, &d);
    return rdmsr(0x8b) >> 32;
  }

  static void cpuid(Unsigned32 mode, Unsigned32 ecx_val, Unsigned32 *eax,
                    Unsigned32 *ebx, Unsigned32 *ecx, Unsigned32 *edx)
  { Proc::cpuid(mode, ecx_val, eax, ebx, ecx, edx); }

  static void cpuid(Unsigned32 mode, Unsigned32 *eax, Unsigned32 *ebx,
                    Unsigned32 *ecx, Unsigned32 *edx)
  { Proc::cpuid(mode, 0, eax, ebx, ecx, edx); }

  static Unsigned32 cpuid_eax(Unsigned32 mode)
  { return Proc::cpuid_eax(mode); }

  static Unsigned32 cpuid_ebx(Unsigned32 mode)
  { return Proc::cpuid_ebx(mode); }

  static Unsigned32 cpuid_ecx(Unsigned32 mode)
  { return Proc::cpuid_ecx(mode); }

  static Unsigned32 cpuid_edx(Unsigned32 mode)
  { return Proc::cpuid_edx(mode); }

  static void set_cr0(unsigned long val)
  { asm volatile ("mov %0, %%cr0" : : "r" (val)); }

  static void set_pdbr(unsigned long addr)
  { asm volatile ("mov %0, %%cr3" : : "r" (addr)); }

  static void set_cr4(unsigned long val)
  { asm volatile ("mov %0, %%cr4" : : "r" (val)); }

  static void set_ldt(Unsigned16 val)
  { asm volatile ("lldt %0" : : "rm" (val)); }

  static void set_ss(Unsigned16 val)
  { asm volatile ("mov %0, %%ss" : : "r" (val)); }

  static void set_tr(Unsigned16 val)
  { asm volatile ("ltr %0" : : "rm" (val)); }

  static void xsetbv(Unsigned64 val, Unsigned32 xcr)
  {
    asm volatile ("xsetbv" : : "a" (static_cast<Mword>(val)),
                               "d" (static_cast<Mword>(val >> 32)),
                               "c" (xcr));
  }

  static Unsigned64 xgetbv(Unsigned32 xcr)
  {
    Unsigned32 eax, edx;
    asm volatile("xgetbv"
                 : "=a" (eax),
                   "=d" (edx)
                 : "c" (xcr));
    return eax | (Unsigned64{edx} << 32);
  }

  static Mword get_cr0()
  {
    Mword val;
    asm volatile ("mov %%cr0, %0" : "=r" (val));
    return val;
  }

  static Address get_pdbr()
  { Address addr; asm volatile ("mov %%cr3, %0" : "=r" (addr)); return addr; }

  static Mword get_cr4()
  { Mword val; asm volatile ("mov %%cr4, %0" : "=r" (val)); return val; }

  static Unsigned16 get_ldt()
  { Unsigned16 val; asm volatile ("sldt %0" : "=rm" (val)); return val; }

  static Unsigned16 get_tr()
  { Unsigned16 val; asm volatile ("str %0" : "=rm" (val)); return val; }

  int can_wrmsr() const
  { return features() & FEAT_MSR; }

  static Unsigned64 rdmsr(Unsigned32 reg)
  { return Proc::rdmsr(reg); }

  static Unsigned64 rdpmc(Unsigned32 idx, Unsigned32)
  {
    Unsigned32 l,h;

    asm volatile ("rdpmc" : "=a" (l), "=d" (h) : "c" (idx));
    return (Unsigned64{h} << 32) + Unsigned64{l};
  }

  static void wrmsr(Unsigned32 low, Unsigned32 high, Unsigned32 reg)
  { Proc::wrmsr((Unsigned64{high} << 32) | low, reg); }

  static void wrmsr(Unsigned64 msr, Unsigned32 reg)
  { Proc::wrmsr(msr, reg); }

  static void enable_rdpmc()
  { set_cr4(get_cr4() | CR4_PCE); }


  // Function used for calculating apic scaler
  static Unsigned32 muldiv(Unsigned32 val, Unsigned32 mul, Unsigned32 div)
  {
    Unsigned32 dummy;

    asm volatile ("mull %3 ; divl %4\n\t"
                 :"=a" (val), "=d" (dummy)
                 : "0" (val),  "d" (mul),  "c" (div));
    return val;
  }


  static Unsigned16 get_cs()
  {
    Unsigned16 val;
    asm volatile ("mov %%cs, %0" : "=rm" (val));
    return val;
  }

  static Unsigned16 get_ds()
  {
    Unsigned16 val;
    asm volatile ("mov %%ds, %0" : "=rm" (val));
    return val;
  }

  static Unsigned16 get_es()
  {
    Unsigned16 val;
    asm volatile ("mov %%es, %0" : "=rm" (val));
    return val;
  }

  static Unsigned16 get_ss()
  {
    Unsigned16 val;
    asm volatile ("mov %%ss, %0" : "=rm" (val));
    return val;
  }

  static void set_ds(Unsigned16 val)
  {
    if (__builtin_constant_p(val))
      FIASCO_IA32_LOAD_SEG_SAFE(ds, val);
    else
      FIASCO_IA32_LOAD_SEG(ds, val);
  }

  static void set_es(Unsigned16 val)
  {
    if (__builtin_constant_p(val))
      FIASCO_IA32_LOAD_SEG_SAFE(es, val);
    else
      FIASCO_IA32_LOAD_SEG(es, val);
  }

  static Unsigned16 get_fs()
  { Unsigned16 val; asm volatile ("mov %%fs, %0" : "=rm" (val)); return val; }

  static Unsigned16 get_gs()
  { Unsigned16 val; asm volatile ("mov %%gs, %0" : "=rm" (val)); return val; }

  static void set_fs(Unsigned16 val)
  {
    if (__builtin_constant_p(val))
      FIASCO_IA32_LOAD_SEG_SAFE(fs, val);
    else
      FIASCO_IA32_LOAD_SEG(fs, val);
  }

  static void set_gs(Unsigned16 val)
  {
    if (__builtin_constant_p(val))
      FIASCO_IA32_LOAD_SEG_SAFE(gs, val);
    else
      FIASCO_IA32_LOAD_SEG(gs, val);
  }

  void debugctl_enable()
  {
    if (debugctl_busy)
      wrmsr(debugctl_set, MSR_DEBUGCTLA);
  }

  void debugctl_disable()
  {
    if (debugctl_busy)
      wrmsr(debugctl_reset, MSR_DEBUGCTLA);
  }

  static char const *exception_string(Mword trapno);

  FIASCO_INIT_CPU
  void arch_perfmon_info(Unsigned32 *eax, Unsigned32 *ebx, Unsigned32 *ecx,
                         Unsigned32 *edx) const
  {
    *eax = _arch_perfmon_info_eax;
    *ebx = _arch_perfmon_info_ebx;
    *ecx = _arch_perfmon_info_ecx;
    *edx = _arch_perfmon_info_edx;
  }

  static unsigned amd_cpuid_mnc();
  void lbr_enable(bool on)
  {
    if (lbr_type() != Lbr_unsupported)
      {
        if (on)
          {
            lbr_active    = true;
            debugctl_set |= 1;
            debugctl_busy = true;
          }
        else
          {
            lbr_active    = false;
            debugctl_set &= ~1;
            debugctl_busy = lbr_active || bts_active;
            wrmsr(debugctl_reset, MSR_DEBUGCTLA);
          }
      }
  }


  void btf_enable(bool on)
  {
    if (lbr_type() != Lbr_unsupported)
      {
        if (on)
          {
            btf_active      = true;
            debugctl_set   |= 2;
            debugctl_reset |= 2; /* don't disable bit in kernel */
            wrmsr(2, MSR_DEBUGCTLA);     /* activate _now_ */
          }
        else
          {
            btf_active    = false;
            debugctl_set &= ~2;
            debugctl_busy = lbr_active || bts_active;
            wrmsr(debugctl_reset, MSR_DEBUGCTLA);
          }
      }
  }

  void bts_enable(bool on)
  {
    if (bts_type() != Bts_unsupported)
      {
        if (on)
          {
            switch (bts_type())
              {
              case Bts_pentium_4: bts_active = true; debugctl_set |= 0x0c; break;
              case Bts_pentium_m: bts_active = true; debugctl_set |= 0xc0; break;
              default:;
              }
            debugctl_busy = lbr_active || bts_active;
          }
        else
          {
            bts_active = false;
            switch (bts_type())
              {
              case Bts_pentium_4: debugctl_set &= ~0x0c; break;
              case Bts_pentium_m: debugctl_set &= ~0xc0; break;
              default:;
              }
            debugctl_busy = lbr_active || bts_active;
            wrmsr(debugctl_reset, MSR_DEBUGCTLA);
          }
      }
  }

protected:
  void cache_tlb_intel();
  void set_frequency_and_scalers(Unsigned64 freq);
  bool tsc_frequency_from_cpuid_15h(bool check_only = false);
  void set_model_str();
  void cache_tlb_l1();
  void cache_tlb_l2_l3();
  void addr_size_info();

  Unsigned64 _frequency;
  Unsigned32 _version;                  // CPUID(1).EAX
  Unsigned32 _brand;                    // CPUID(1).EBX
  Unsigned32 _ext_features;             // CPUID(1).ECX
  Unsigned32 _features;                 // CPUID(1).EDX
  Unsigned32 _ext_07_ebx;               // CPUID(7).EBX
  Unsigned32 _ext_07_edx;               // CPUID(7).EDX
  Unsigned32 _ext_8000_0001_ecx;        // CPUID(8000_0001).ECX
  Unsigned32 _ext_8000_0001_edx;        // CPUID(8000_0001).EDX
  Unsigned32 _local_features;           // See Local_features
  Unsigned64 _arch_capabilities;        // MSR_IA32_ARCH_CAPABILITIES

  Unsigned16 _inst_tlb_4k_entries;
  Unsigned16 _data_tlb_4k_entries;
  Unsigned16 _inst_tlb_4m_entries;
  Unsigned16 _data_tlb_4m_entries;
  Unsigned16 _inst_tlb_4k_4m_entries;
  Unsigned16 _data_tlb_4k_4m_entries;
  Unsigned16 _l2_inst_tlb_4k_entries;
  Unsigned16 _l2_data_tlb_4k_entries;
  Unsigned16 _l2_inst_tlb_4m_entries;
  Unsigned16 _l2_data_tlb_4m_entries;

  Unsigned16 _l1_trace_cache_size;
  Unsigned16 _l1_trace_cache_asso;

  Unsigned16 _l1_data_cache_size;
  Unsigned16 _l1_data_cache_asso;
  Unsigned16 _l1_data_cache_line_size;

  Unsigned16 _l1_inst_cache_size;
  Unsigned16 _l1_inst_cache_asso;
  Unsigned16 _l1_inst_cache_line_size;

  Unsigned16 _l2_cache_size;
  Unsigned16 _l2_cache_asso;
  Unsigned16 _l2_cache_line_size;

  Unsigned32 _l3_cache_size;
  Unsigned16 _l3_cache_asso;
  Unsigned16 _l3_cache_line_size;

  Unsigned8 _phys_bits;
  Unsigned8 _virt_bits;

  Vendor _vendor;
  char _model_str[52];

  Unsigned32 _arch_perfmon_info_eax;    // CPUID(10).EAX
  Unsigned32 _arch_perfmon_info_ebx;    // CPUID(10).EBX
  Unsigned32 _arch_perfmon_info_ecx;    // CPUID(10).ECX
  Unsigned32 _arch_perfmon_info_edx;    // CPUID(10).EDX

  Unsigned32 _monitor_mwait_eax;        // CPUID(5).EAX
  Unsigned32 _monitor_mwait_ebx;        // CPUID(5).EBX
  Unsigned32 _monitor_mwait_ecx;        // CPUID(5).ECX
  Unsigned32 _monitor_mwait_edx;        // CPUID(5).EDX

  Unsigned32 _thermal_and_pm_eax;       // CPUID(6).EAX

  static Unsigned32 scaler_tsc_to_ns;
  static Unsigned32 scaler_tsc_to_us;
  static Unsigned32 scaler_ns_to_tsc;

  struct Vendor_table {
    Unsigned32 vendor_mask;
    Unsigned32 vendor_code;
    Unsigned16 l2_cache;
    char       vendor_string[32];
  } __attribute__((packed));

  struct Cache_table {
    Unsigned8  desc;
    Unsigned8  level;
    Unsigned16 size;
    Unsigned8  asso;
    Unsigned8  line_size;
  };

  static Vendor_table const intel_table[];
  static Vendor_table const amd_table[];
  static Vendor_table const cyrix_table[];
  static Vendor_table const via_table[];
  static Vendor_table const umc_table[];
  static Vendor_table const nexgen_table[];
  static Vendor_table const rise_table[];
  static Vendor_table const transmeta_table[];
  static Vendor_table const sis_table[];
  static Vendor_table const nsc_table[];

  static Cache_table const intel_cache_table[];

  static char const * const vendor_ident[];
  static Vendor_table const * const vendor_table[];

  static char const * const exception_strings[];

  void init_indirect_branch_mitigation();
  void try_enable_hw_performance_states(bool resume);

  /** Flags if lbr or bts facilities are activated, used by double-fault
   *  handler to reset the debugging facilities
   */
  Unsigned32 debugctl_busy;

  /** debugctl value for activating lbr or bts */
  Unsigned32 debugctl_set;

  /** debugctl value to reset activated lr/bts facilities in the double-fault
   *  handler
   */
  Unsigned32 debugctl_reset;

  /** supported lbr type */
  Lbr _lbr;

  /** supported bts type */
  Bts _bts;

  /** is lbr active ? */
  char lbr_active;

  /** is btf active ? */
  char btf_active;

  /** is bts active ? */
  char bts_active;

  Gdt *gdt;
  Tss *tss;
  Tss *tss_dbf;

  void init_lbr_type();
  void init_bts_type();

  Unsigned64 _suspend_tsc;
};
