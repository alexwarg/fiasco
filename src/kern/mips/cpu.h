#pragma once

#include "per_cpu_data.h"
#include "cpu_generic.h"
#include "initcalls.h"
#include "processor.h"
#include "types.h"
#include <cxx/bitfield>

namespace Mips {
  struct Cfg_base
  {
    Unsigned32 _v;
    Cfg_base() = default;
    Cfg_base(Unsigned32 v) : _v(v) {}
    CXX_BITFIELD_MEMBER(31, 31, m, _v);
  };

  template<unsigned IDX> struct Cfg;
  template<> struct Cfg<0> : Cfg_base
  {
    Cfg() = default;
    Cfg(Unsigned32 v) : Cfg_base(v) {}
    CXX_BITFIELD_MEMBER( 0,  2, k0, _v);
    CXX_BITFIELD_MEMBER( 3,  3, vi, _v);
    CXX_BITFIELD_MEMBER( 7,  9, mt, _v);
    CXX_BITFIELD_MEMBER(10, 12, ar, _v);
    CXX_BITFIELD_MEMBER(13, 14, at, _v);
    CXX_BITFIELD_MEMBER(15, 15, be, _v);
    CXX_BITFIELD_MEMBER(16, 24, impl, _v);
    CXX_BITFIELD_MEMBER(25, 27, ku, _v);
    CXX_BITFIELD_MEMBER(28, 30, k23, _v);
    static Cfg<0> read() { return mfc0_32(16, 0); }
  };

  template<> struct Cfg<1> : Cfg_base
  {
    Cfg() = default;
    Cfg(Unsigned32 v) : Cfg_base(v) {}
    CXX_BITFIELD_MEMBER( 0,  0, fp, _v);
    CXX_BITFIELD_MEMBER( 1,  1, ep, _v);
    CXX_BITFIELD_MEMBER( 2,  2, ca, _v);
    CXX_BITFIELD_MEMBER( 3,  3, wr, _v);
    CXX_BITFIELD_MEMBER( 4,  4, pc, _v);
    CXX_BITFIELD_MEMBER( 5,  5, md, _v);
    CXX_BITFIELD_MEMBER( 6,  6, c2, _v);
    CXX_BITFIELD_MEMBER( 7,  9, da, _v);
    CXX_BITFIELD_MEMBER(10, 12, dl, _v);
    CXX_BITFIELD_MEMBER(13, 15, ds, _v);
    CXX_BITFIELD_MEMBER(16, 18, ia, _v);
    CXX_BITFIELD_MEMBER(19, 21, il, _v);
    CXX_BITFIELD_MEMBER(22, 24, is, _v);
    CXX_BITFIELD_MEMBER(25, 30, mmu_size, _v);
    static Cfg<1> read() { return mfc0_32(16, 1); }
  };

  template<> struct Cfg<2> : Cfg_base
  {
    Cfg() = default;
    Cfg(Unsigned32 v) : Cfg_base(v) {}
    CXX_BITFIELD_MEMBER( 0,  3, sa, _v);
    CXX_BITFIELD_MEMBER( 4,  7, sl, _v);
    CXX_BITFIELD_MEMBER( 8, 11, ss, _v);
    CXX_BITFIELD_MEMBER(12, 15, su, _v);
    CXX_BITFIELD_MEMBER(16, 19, ta, _v);
    CXX_BITFIELD_MEMBER(20, 23, tl, _v);
    CXX_BITFIELD_MEMBER(24, 27, ts, _v);
    CXX_BITFIELD_MEMBER(28, 30, tu, _v);
    static Cfg<2> read() { return mfc0_32(16, 2); }
  };

  template<> struct Cfg<3> : Cfg_base
  {
    Cfg() = default;
    Cfg(Unsigned32 v) : Cfg_base(v) {}
    CXX_BITFIELD_MEMBER( 0,  0, tl, _v);
    CXX_BITFIELD_MEMBER( 1,  1, sm, _v);
    CXX_BITFIELD_MEMBER( 2,  2, mt, _v);
    CXX_BITFIELD_MEMBER( 3,  3, cdmm, _v);
    CXX_BITFIELD_MEMBER( 4,  4, sp, _v);
    CXX_BITFIELD_MEMBER( 5,  5, vint, _v);
    CXX_BITFIELD_MEMBER( 6,  6, veic, _v);
    CXX_BITFIELD_MEMBER( 7,  7, lpa, _v);
    CXX_BITFIELD_MEMBER( 8,  8, itl, _v);
    CXX_BITFIELD_MEMBER( 9,  9, ctxtc, _v);
    CXX_BITFIELD_MEMBER(10, 10, dspp, _v);
    CXX_BITFIELD_MEMBER(11, 11, dsp2p, _v);
    CXX_BITFIELD_MEMBER(12, 12, rxi, _v);
    CXX_BITFIELD_MEMBER(13, 13, ulri, _v);
    CXX_BITFIELD_MEMBER(14, 15, isa, _v);
    CXX_BITFIELD_MEMBER(16, 16, isa_on_exc, _v);
    CXX_BITFIELD_MEMBER(17, 17, mcu, _v);
    CXX_BITFIELD_MEMBER(18, 20, mmar, _v);
    CXX_BITFIELD_MEMBER(21, 22, iplw, _v);
    CXX_BITFIELD_MEMBER(23, 23, vz, _v);
    CXX_BITFIELD_MEMBER(24, 24, pw, _v);
    CXX_BITFIELD_MEMBER(25, 25, sc, _v);
    CXX_BITFIELD_MEMBER(26, 26, bi, _v);
    CXX_BITFIELD_MEMBER(27, 27, bp, _v);
    CXX_BITFIELD_MEMBER(28, 28, msap, _v);
    CXX_BITFIELD_MEMBER(29, 29, cmgcr, _v);
    CXX_BITFIELD_MEMBER(30, 30, bpg, _v);
    static Cfg<3> read() { return mfc0_32(16, 3); }
  };

  template<> struct Cfg<4> : Cfg_base
  {
    Cfg() = default;
    Cfg(Unsigned32 v) : Cfg_base(v) {}
    CXX_BITFIELD_MEMBER( 0,  7, mmu_sz_ext, _v);
    CXX_BITFIELD_MEMBER( 0,  3, ftlb_sets, _v);
    CXX_BITFIELD_MEMBER( 4,  7, ftlb_ways, _v);
    CXX_BITFIELD_MEMBER( 0,  7, ftlb_info, _v);
    CXX_BITFIELD_MEMBER( 8, 12, ftlb_page_size2, _v);
    CXX_BITFIELD_MEMBER( 8, 10, ftlb_page_size1, _v);
    CXX_BITFIELD_MEMBER(14, 15, mmu_ext_def, _v);
    CXX_BITFIELD_MEMBER(16, 23, k_scr_num, _v);
    CXX_BITFIELD_MEMBER(24, 27, vtlb_sz_ext, _v);
    CXX_BITFIELD_MEMBER(28, 28, ae, _v);
    CXX_BITFIELD_MEMBER(29, 30, ie, _v);
    static Cfg<4> read() { return mfc0_32(16, 4); }
  };

  template<> struct Cfg<5> : Cfg_base
  {
    Cfg() = default;
    Cfg(Unsigned32 v) : Cfg_base(v) {}
    CXX_BITFIELD_MEMBER( 0,  0, nf_exists, _v);
    CXX_BITFIELD_MEMBER( 2,  2, ufr, _v);
    CXX_BITFIELD_MEMBER( 3,  3, mrp, _v);
    CXX_BITFIELD_MEMBER( 4,  4, llb, _v);
    CXX_BITFIELD_MEMBER( 5,  5, mvh, _v);
    CXX_BITFIELD_MEMBER( 6,  6, sbri, _v);
    CXX_BITFIELD_MEMBER( 7,  7, vp, _v);
    CXX_BITFIELD_MEMBER( 8,  8, fre, _v);
    CXX_BITFIELD_MEMBER( 9,  9, ufe, _v);
    CXX_BITFIELD_MEMBER(10, 10, l2c, _v);
    CXX_BITFIELD_MEMBER(11, 11, dec, _v);
    CXX_BITFIELD_MEMBER(13, 13, xnp, _v);
    CXX_BITFIELD_MEMBER(27, 27, msa_en, _v);
    CXX_BITFIELD_MEMBER(28, 28, eva, _v);
    CXX_BITFIELD_MEMBER(29, 29, cv, _v);
    CXX_BITFIELD_MEMBER(30, 30, k, _v);
    static Cfg<5> read() { return mfc0_32(16, 5); }
  };

  struct Configs
  {
    Cfg_base c[6];

    template<unsigned IDX> Cfg<IDX> r() const { return Cfg<IDX>(c[IDX]._v); }
    template<unsigned IDX> Cfg<IDX> &r() { return static_cast<Cfg<IDX>&>(c[IDX]); }

    template<unsigned IDX>
    void read_reg()
    {
      if (IDX == 0 || c[IDX - 1].m())
        c[IDX] = Cfg<IDX>::read();
      else
        c[IDX] = 0;
    }

    void read_all()
    {
      read_reg<0>();
      read_reg<1>();
      read_reg<2>();
      read_reg<3>();
      read_reg<4>();
      read_reg<5>();
    }

    static Configs read()
    {
      Configs c;
      c.read_all();
      return c;
    }
  };
}

/**
 * Define an entry in the MIPS CPU specific hooks table.
 * \param id_mask  32bit mask value to mask bits in the ProcId that shall
 *                 not be matched (cleared bits are cleared before the match).
 * \param id       32bit ID value that is compared against the masked ProcId
 *                 value.
 * \param hooks    Pointer to a Cpu::Hooks object that is called on a match.
 *
 * The table contains mask and id fields that are matched against
 * the Processor ID CP0 register and called if a match is found.
 */
#define DEFINE_MIPS_CPU_TYPE(id_mask, id, hooks) \
  static Cpu::Cpu_type const __attribute__((used, section(".mips.cpu_type"))) \
    _mips_cpu_type_##__COUNTER__ = { id_mask, id, hooks }

class Cpu : public Cpu_generic
{
public:
  /**
   * Abstract hooks interface where hooks are called for
   * matching CPUs.
   */
  struct Hooks
  {
    virtual void init(Cpu_number, bool resume, Unsigned32 prid) = 0;
  };

  /// Entry in the CPU hooks table.
  struct Cpu_type
  {
    Unsigned32 id_mask;
    Unsigned32 id;
    Hooks *hooks;
  };

  void init(Cpu_number cpu, bool resume, bool is_boot_cpu = false);

  static Unsigned64 const _frequency
    = (Unsigned64)Config::Cpu_frequency * 1000000;

  static Unsigned64 frequency() { return _frequency; }
  static Per_cpu<Cpu> cpus;
  static Cpu *boot_cpu() { return _boot_cpu; }

  Cpu(Cpu_number cpu) { set_id(cpu); }

  static Mword stack_align(Mword stack)
  { return stack & ~0x3; }

  Cpu_phys_id phys_id() const
  { return _phys_id; }

  static Unsigned64 rdtsc()
  { return (Unsigned32)(Mips::mfc0_32(9, 0)); }

  static unsigned phys_bits()
  {
    // FIXME: store phys bits on init and return those
    // FIXME: currently limiting phys bits to 48
    return MWORD_BITS <= 32 ? MWORD_BITS : 48;
  }

  static void debugctl_enable() {}
  static void debugctl_disable() {}
  static Unsigned32 get_scaler_tsc_to_ns() { return 0; }
  static Unsigned32 get_scaler_tsc_to_us() { return 0; }
  static Unsigned32 get_scaler_ns_to_tsc() { return 0; }

  struct Options
  {
    Mword _o;
    CXX_BITFIELD_MEMBER (0, 0, tlbinv, _o);
    CXX_BITFIELD_MEMBER (1, 1, ulr,    _o);
    CXX_BITFIELD_MEMBER (2, 2, vz,     _o);
    CXX_BITFIELD_MEMBER (3, 3, bi,     _o); /// < BadInstr supported
    CXX_BITFIELD_MEMBER (4, 4, bp,     _o); /// < BadInstrP supported
    CXX_BITFIELD_MEMBER (5, 5, hwpw,   _o); /// < HW page walk
    CXX_BITFIELD_MEMBER (6, 6, ftlb,   _o); /// < Dual VTLB / FTLB found
    CXX_BITFIELD_MEMBER (7, 7, ftlbinv,_o); /// < Dual VTLB / FTLB full TLBINV
    CXX_BITFIELD_MEMBER (8, 8, segctl, _o); /// < Segmentation control
  };


  static unsigned tlb_size() { return _tlb_size; }
  static unsigned ftlb_sets() { return _ftlb_sets; }
  static unsigned ftlb_ways() { return _ftlb_ways; }
  static Options options;

private:
  friend struct Cpu_type;

  static Cpu_type _types[] __asm__("MIPS_cpu_types");
  static Cpu *_boot_cpu;
  static unsigned long _ns_per_cycle;
  static unsigned _tlb_size;
  static unsigned _ftlb_sets;
  static unsigned _ftlb_ways;
  static unsigned _default_cca;

  Cpu_phys_id _phys_id;

  void panic(char const *fmt, ...) const
    __attribute__((noreturn, format(printf,2,3)));
  void require(bool cond, char const *fmt, ...) const
    __attribute__((format(printf,3,4)));
  void pr(char const *fmt, ...) const
    __attribute__((format(printf,2,3)));

  bool if_show_infos() const;
  void print_infos() const;
  void first_boot(bool is_boot_cpu);
};
