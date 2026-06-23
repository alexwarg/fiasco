#pragma once

#include <entry_frame.h>
#include <per_cpu_data.h>
#include <cpu.h>
#include <timer.h> // assumes timer-mips CP0 timer

class Context;

class Vz
{
public:
  struct Options
  {
    unsigned ctl_0_ext:1;
    unsigned ctl_1    :1;
    unsigned ctl_2    :1;
  };

  struct Owner
  {
    Context *ctxt = nullptr;
    int guest_id = -1;
  };

  static Per_cpu<Owner> owner;

  struct State
  {
    Mword version;
    Mword size;

    Mword ctl_0_ext;
    Mword ctl_gtoffset;
    Mword ctl_1; ///< not exported, probably needed for EID
    Mword ctl_2;
    // Mword ctl_3; ///< currently no shadow sets are exported

    /**
     * Dirty cp0 register map..
     * On VM exit each bit corresponds to an updated register.
     * On VM entry each 1 bit means state that was changed by the VMM
     * and needs to be loaded into HW.
     */
    Unsigned32 current_cp0_map;
    Unsigned32 modified_cp0_map;

    // Status and Ctl0 are always saved on exit
    Mword ctl_0;
    Mword g_status;      // $12, 0

    // The timestamp and the cause are always saved together
    Unsigned64 _saved_cause_timestamp;
    Mword g_cause;       // $13, 0
    Mword g_compare;     // $11, 0

    Mword g_cfg[6];      // Guest.Config[0..5]

    Mword g_index;       // $0, 0
    Mword g_entry_lo[2]; // $2 and $3
    Mword g_context;     // $4, 0
    Mword g_page_mask;   // $5, 0
    Mword g_wired;       // $6, 0
    Mword g_entry_hi;    // $10, 0

    Mword g_page_grain;  // $5, 1
    Mword g_seg_ctl[3];  // $5, 2 .. 4

    Mword g_pw_base;     // $5, 5
    Mword g_pw_field;    // $5, 6
    Mword g_pw_size;     // $5, 7
    Mword g_pw_ctl;      // $6, 6

    Mword g_intctl;      // $12, 1
    Mword g_ulr;         // $4, 2
    Mword g_epc;         // $14, 0
    Mword g_error_epc;   // $30, 0
    Mword g_ebase;       // $15, 1

    Mword g_hwrena;      // $7, 0
    Mword g_bad_v_addr;  // $8, 0
    Mword g_bad_instr;   // $8, 1
    Mword g_bad_instr_p; // $8, 2

    Mword g_kscr[8];     // $31, 0 .. 7


    struct Tlb_entry
    {
      Mword mask;
      Mword entry_hi;
      Mword entry_lo[2];
    };

    enum { Max_guest_wired = 16 };
    Tlb_entry g_tlb_wired[Max_guest_wired];

    enum Modified_bits
    {
      M_cfg       = 1UL <<  0, ///< Guest.Config registers
      M_mmu       = 1UL <<  1, ///< MMU registers
      M_xlat      = 1UL <<  2, ///< Address translation registers
      M_pw        = 1UL <<  3, ///< Page walker registers
      M_bad       = 1UL <<  4, ///< BadVAddr, BadInstr, BadInstrP
      M_kscr      = 1UL <<  5, ///< KScr0..7
      M_status    = 1UL <<  6, ///< Status
      M_cause     = 1UL <<  7, ///< Cause
      M_epc       = 1UL <<  8, ///< EPC, ErrorEPC
      M_hwrena    = 1UL <<  9, ///< HWRena
      M_intctl    = 1UL << 10, ///< IntCtl
      M_ebase     = 1UL << 11, ///< EBase
      M_ulr       = 1UL << 12, ///< ULR
      M_ctl_0     = 1UL << 16, ///< GuestCtl0
      M_ctl_0_ext = 1UL << 17, ///< GuestCtl0Ext
      M_ctl_2     = 1UL << 18, ///< GuestCtl2
      M_gtoffset  = 1UL << 24, ///< GTOffset
      M_compare   = 1UL << 25, ///< GuestCompare
      M_llbit     = 1UL << 26, ///< when set, clear LLBit in LLAddr register
    };

    void init();
    void load_ctl() const
    {
      using namespace Mips;

      auto c0 = access_once(&ctl_0);
      c0 &= Ctl_0_mbz;
      c0 |= Ctl_0_mb1;
      mtc0_32(c0, Cp0_guest_ctl_0);
      mtc0_32(ctl_gtoffset, Cp0_gt_offset);

      if (Vz::options.ctl_0_ext)
        {
          auto c0 = access_once(&ctl_0_ext);
          c0 &= Ctl_0_ext_mbz;
          c0 |= Ctl_0_ext_mb1;
          mtc0_32(c0, Cp0_guest_ctl_0_ext);
        }

      if (Vz::options.ctl_2)
        {
          auto c2 = access_once(&ctl_2);
          c2 &= Ctl_2_mbz;
          mtc0_32(c2, Cp0_guest_ctl_2);
        }
    }

    void save_ctl()
    {
      ctl_0 = Mips::mfc0_32(Mips::Cp0_guest_ctl_0);
    }

    [[gnu::always_inline]]
    void mfg_kscr(unsigned kscr_n, unsigned x)
    {
      if (kscr_n & (1 << x))
        Mips::mfgc0(&g_kscr[x], 31, x);
    }

    [[gnu::always_inline]]
    void mtg_kscr(unsigned kscr_n, unsigned x) const
    {
      if (kscr_n & (1 << x))
        Mips::mtgc0(g_kscr[x], 31, x);
    }

    [[gnu::always_inline]]
    void mtg_cfg(unsigned x) const
    {
      if ((x == 0) || guest_cfg.c[x - 1].m())
        {
          Mword w_mask = guest_cfg_write.c[x]._v;
          Mword v = access_once(&g_cfg[x]);
          v = (v & w_mask) | (guest_cfg.c[x]._v & ~w_mask);
          Mips::mtgc0_32(v, 16, x);
        }
    }

    void save_full(int guest_id);
    void load_full(int guest_id);
    void load_selective(int guest_id);
    void save_on_exit(Entry_frame::Cause cause)
    {
      (void) cause;
      auto c_map = current_cp0_map;
      if (!(c_map & M_ctl_0))
        save_ctl();   // save GuestCtl0

      if (!(c_map & M_status))
        Mips::mfgc0_32(&g_status, Mips::Cp0_status);

      write_now(&current_cp0_map, c_map | M_ctl_0 | M_status);
    }

    void update_cause_ti()
    {
      enum { Cause_TI = 1UL << 30 };

      if (g_cause & Cause_TI)
        return;

      Unsigned64 ct = Timer::get_current_counter();
      Unsigned64 gc = ct + (Signed32) ctl_gtoffset;
      Unsigned64 last_gc = _saved_cause_timestamp + (Signed32) ctl_gtoffset;
      Unsigned64 gcomp = ((Unsigned32) g_compare) | (last_gc & 0xffffffff00000000);

      if (gcomp < last_gc)
        gcomp += 0x100000000;

      if (gcomp <= gc)
        g_cause |= Cause_TI;
    }

  private:
    void load_cause();
    void load_guest_tlb_entry(int guest_id, unsigned i);
    void save_guest_tlb_entry(int guest_id, unsigned i);

    enum
    {
      Ctl_0_mbz  = 0x7f7f0281, ///< must be zero mask CTL0
      Ctl_0_mb1  = 0x0c000000, ///< must be one bits CTL0
      Ctl_0_dflz = Ctl_0_mbz & 0x1c7f0280, ///< default zero mask CTL0
      Ctl_0_dfl1 = Ctl_0_mb1 | 0x1c000000, ///< default one bits CTL0

      Ctl_0_ext_mbz = 0x0d7,
      Ctl_0_ext_mb1 = 0x000,
      Ctl_0_ext_dflz = Ctl_0_ext_mbz & 0x0c0,
      Ctl_0_ext_dfl1 = Ctl_0_ext_mb1 | 0x000,

      Ctl_2_mbz = 0xfc00,
    };

  };

  static void init();

  static Mips::Configs const *guest_config()
  { return &guest_cfg; }

  static Mips::Configs const *guest_config_write()
  { return &guest_cfg_write; }

private:
  static Mips::Configs guest_cfg;
  static Mips::Configs guest_cfg_write;

  static Options options;

  [[gnu::always_inline]]
  static void mfg_cfg_init(unsigned x)
  {
    if ((x == 0) || guest_cfg.c[x - 1].m())
      guest_cfg.c[x]._v = Mips::mfgc0_32(16, x);
  }
};

