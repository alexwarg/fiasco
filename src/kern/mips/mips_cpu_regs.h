#pragma once

#include <types.h>
#include <globalconfig.h>

#define FIASCO_CP0_SIZE_ERROR(size, reg, sel)              \
  asm (".error \"" __FILE__ ":" FIASCO_STRINGIFY(__LINE__) \
       ": CP0 register %0, %1 is not " size " sized\""     \
       : : "i"(reg), "i"(sel))

namespace Mips
{
  enum Cp0_regs
  {
    /* 32 */ Cp0_index            = ( 0 << 3),
    /* 32 */ Cp0_vp_control       = ( 0 << 3) + 4,
    /* 32 */ Cp0_random           = ( 1 << 3),
    /* 64 */ Cp0_entry_lo1        = ( 2 << 3),
    /* 64 */ Cp0_entry_lo2        = ( 3 << 3),
    /* 32 */ Cp0_global_number    = ( 3 << 3) + 1,
    /* 64 */ Cp0_context          = ( 4 << 3),
    /* 32 */ Cp0_context_config   = ( 4 << 3) + 1,
    /* 64 */ Cp0_user_local       = ( 4 << 3) + 2,
    /* 64 */ Cp0_xcontext_config  = ( 4 << 3) + 3,
    /* 32 */ Cp0_debug_context_id = ( 4 << 3) + 4,
    /* 64 */ Cp0_page_mask        = ( 5 << 3),
    /* 32 */ Cp0_page_grain       = ( 5 << 3) + 1,
    /* 64 */ Cp0_seg_ctl_0        = ( 5 << 3) + 2,
    /* 64 */ Cp0_seg_ctl_1        = ( 5 << 3) + 3,
    /* 64 */ Cp0_seg_ctl_2        = ( 5 << 3) + 4,
    /* 64 */ Cp0_pw_base          = ( 5 << 3) + 5,
    /* 64 */ Cp0_pw_field         = ( 5 << 3) + 6,
    /* 64 */ Cp0_pw_size          = ( 5 << 3) + 7,
    /* 32 */ Cp0_wired            = ( 6 << 3),
    /* 32 */ Cp0_pw_ctl           = ( 6 << 3) + 6,
    /* 32 */ Cp0_hw_rena          = ( 7 << 3),
    /* 64 */ Cp0_bad_v_addr       = ( 8 << 3),
    /* 32 */ Cp0_bad_instr        = ( 8 << 3) + 1,
    /* 32 */ Cp0_bad_instr_p      = ( 8 << 3) + 2,
    /* 32 */ Cp0_count            = ( 9 << 3),
    /* 64 */ Cp0_entry_hi         = (10 << 3),
    /* 32 */ Cp0_guest_ctl_1      = (10 << 3) + 4,
    /* 32 */ Cp0_guest_ctl_2      = (10 << 3) + 5,
    /* 32 */ Cp0_guest_ctl_3      = (10 << 3) + 6,
    /* 32 */ Cp0_compare          = (11 << 3),
    /* 32 */ Cp0_guest_ctl_0_ext  = (11 << 3) + 4,
    /* 32 */ Cp0_status           = (12 << 3),
    /* 32 */ Cp0_int_ctl          = (12 << 3) + 1,
    /* 32 */ Cp0_srs_ctl          = (12 << 3) + 2,
    /* 32 */ Cp0_srs_map          = (12 << 3) + 3,
    /* 32 */ Cp0_guest_ctl_0      = (12 << 3) + 6,
    /* 32 */ Cp0_gt_offset        = (12 << 3) + 7,
    /* 32 */ Cp0_cause            = (13 << 3),
    /* 32 */ Cp0_nested_exc       = (13 << 3) + 5,
    /* 64 */ Cp0_epc              = (14 << 3),
    /* 64 */ Cp0_nested_epc       = (14 << 3) + 2,
    /* 32 */ Cp0_proc_id          = (15 << 3),
    /* 64 */ Cp0_ebase            = (15 << 3) + 1,
    /* 64 */ Cp0_cdmm_base        = (15 << 3) + 2,
    /* 64 */ Cp0_cmgcr_base       = (15 << 3) + 3,
    /* 32 */ Cp0_bevva            = (15 << 3) + 4,
    /* 32 */ Cp0_config_0         = (16 << 3),
    /* 32 */ Cp0_config_1         = (16 << 3) + 1,
    /* 32 */ Cp0_config_2         = (16 << 3) + 2,
    /* 32 */ Cp0_config_3         = (16 << 3) + 3,
    /* 32 */ Cp0_config_4         = (16 << 3) + 4,
    /* 32 */ Cp0_config_5         = (16 << 3) + 5,
    /* 32 */ Cp0_config_6         = (16 << 3) + 6, ///< unused
    /* 32 */ Cp0_config_7         = (16 << 3) + 7, ///< unused
    /* 64 */ Cp0_load_linked_addr = (17 << 3),
    /* 64 */ Cp0_maar_0           = (17 << 3) + 1,
    /* 64 */ Cp0_maar_1           = (17 << 3) + 2,
    /* 64 */ Cp0_watch_lo         = (18 << 3),
    /* 64 */ Cp0_watch_hi         = (19 << 3),
    /* 64 */ Cp0_xcontext         = (20 << 3),
    /* 64 */ Cp0_debug            = (23 << 3),
    /* 64 */ Cp0_debug_2          = (23 << 3) + 6,
    /* 64 */ Cp0_depc             = (24 << 3),
    /* 32 */ Cp0_perf_ctl_0       = (25 << 3),
    /* 64 */ Cp0_perf_counter_0   = (25 << 3) + 1,
    /* 32 */ Cp0_perf_ctl_1       = (25 << 3) + 2,
    /* 64 */ Cp0_perf_counter_1   = (25 << 3) + 3,
    /* 32 */ Cp0_perf_ctl_2       = (25 << 3) + 4,
    /* 64 */ Cp0_perf_counter_2   = (25 << 3) + 5,
    /* 32 */ Cp0_perf_ctl_3       = (25 << 3) + 6,
    /* 64 */ Cp0_perf_counter_3   = (25 << 3) + 7,
    /* 64 */ Cp0_err_ctl          = (26 << 3),
    /* 64 */ Cp0_cache_err        = (27 << 3),
    /* 64 */ Cp0_tag_lo_0         = (28 << 3),
    /* 64 */ Cp0_data_lo_0        = (28 << 3) + 1,
    /* 64 */ Cp0_tag_lo_1         = (28 << 3) + 2,
    /* 64 */ Cp0_data_lo_1        = (28 << 3) + 3,
    /* 64 */ Cp0_tag_hi_0         = (29 << 3),
    /* 64 */ Cp0_data_hi_0        = (29 << 3) + 1,
    /* 64 */ Cp0_tag_hi_1         = (29 << 3) + 2,
    /* 64 */ Cp0_data_hi_1        = (29 << 3) + 3,
    /* 64 */ Cp0_err_epc          = (30 << 3),
    /* 64 */ Cp0_desave           = (31 << 3),
    /* 64 */ Cp0_kscratch_1       = (31 << 3) + 2,
    /* 64 */ Cp0_kscratch_2       = (31 << 3) + 3,
    /* 64 */ Cp0_kscratch_3       = (31 << 3) + 4,
    /* 64 */ Cp0_kscratch_4       = (31 << 3) + 5,
    /* 64 */ Cp0_kscratch_5       = (31 << 3) + 6,
    /* 64 */ Cp0_kscratch_6       = (31 << 3) + 7,
  };

  [[gnu::always_inline]] constexpr bool is_mword(Cp0_regs r)
  {
    return    (r == Cp0_entry_lo1)
           || (r == Cp0_entry_lo2)
           || (r == Cp0_context)
           || (r == Cp0_user_local)
           || (r == Cp0_xcontext_config)
           || (r == Cp0_seg_ctl_0)
           || (r == Cp0_seg_ctl_1)
           || (r == Cp0_seg_ctl_2)
           || (r == Cp0_pw_base)
           || (r == Cp0_pw_field)
           || (r == Cp0_pw_size)
           || (r == Cp0_bad_v_addr)
           || (r == Cp0_entry_hi)
           || (r == Cp0_epc)
           || (r == Cp0_nested_epc)
           || (r == Cp0_ebase)
           || (r == Cp0_cdmm_base)
           || (r == Cp0_cmgcr_base)
           || (r == Cp0_load_linked_addr)
           || (r == Cp0_watch_lo)
           || (r == Cp0_perf_counter_0)
           || (r == Cp0_perf_counter_1)
           || (r == Cp0_perf_counter_2)
           || (r == Cp0_perf_counter_3)
           || (r == Cp0_xcontext)
           || (r == Cp0_tag_lo_0)
           || (r == Cp0_err_epc)
           || (r == Cp0_desave)
           || (r == Cp0_kscratch_1)
           || (r == Cp0_kscratch_2)
           || (r == Cp0_kscratch_3)
           || (r == Cp0_kscratch_4)
           || (r == Cp0_kscratch_5)
           || (r == Cp0_kscratch_6);
  }

  [[gnu::always_inline]] constexpr bool is_mword(unsigned reg, unsigned sel)
  { return is_mword(Cp0_regs((reg << 3) | sel)); }

  [[gnu::always_inline]] constexpr bool is_32bit(Cp0_regs r)
  {
    return    (r == Cp0_index)
           || (r == Cp0_vp_control)
           || (r == Cp0_random)
           || (r == Cp0_global_number)
           || (r == Cp0_context_config)
           || (r == Cp0_debug_context_id)
           || (r == Cp0_page_mask) // no BPG yet
           || (r == Cp0_page_grain)
           || (r == Cp0_wired)
           || (r == Cp0_pw_ctl)
           || (r == Cp0_hw_rena)
           || (r == Cp0_bad_instr)
           || (r == Cp0_bad_instr_p)
           || (r == Cp0_count)
           || (r == Cp0_guest_ctl_1)
           || (r == Cp0_guest_ctl_2)
           || (r == Cp0_guest_ctl_3)
           || (r == Cp0_compare)
           || (r == Cp0_guest_ctl_0_ext)
           || (r == Cp0_status)
           || (r == Cp0_int_ctl)
           || (r == Cp0_srs_ctl)
           || (r == Cp0_srs_map)
           || (r == Cp0_guest_ctl_0)
           || (r == Cp0_gt_offset)
           || (r == Cp0_cause)
           || (r == Cp0_nested_exc)
           || (r == Cp0_proc_id)
           || (r == Cp0_bevva)
           || (r == Cp0_config_0)
           || (r == Cp0_config_1)
           || (r == Cp0_config_2)
           || (r == Cp0_config_3)
           || (r == Cp0_config_4)
           || (r == Cp0_config_5)
           || (r == Cp0_config_6)
           || (r == Cp0_config_7)
           || (r == Cp0_watch_hi)
           || (r == Cp0_perf_ctl_0)
           || (r == Cp0_perf_counter_0)
           || (r == Cp0_perf_ctl_1)
           || (r == Cp0_perf_counter_1)
           || (r == Cp0_perf_ctl_2)
           || (r == Cp0_perf_counter_2)
           || (r == Cp0_perf_ctl_3)
           || (r == Cp0_perf_counter_3);
  }

  [[gnu::always_inline]] constexpr bool is_32bit(unsigned reg, unsigned sel)
  { return is_32bit(Cp0_regs((reg << 3) | sel)); }

  [[gnu::always_inline]] inline
  Smword
  mfgc0_32(unsigned reg, unsigned sel)
  {
    if (!is_32bit(reg, sel))
      FIASCO_CP0_SIZE_ERROR("32bit", reg, sel);

    Smword v;
    asm volatile (
        ".set push\n\t.set virt\n\t"
        "mfgc0\t%0, $%1, %2\n\t"
        ".set pop"
        : "=r"(v) : "i"(reg), "i"(sel));
    return v;
  }

  [[gnu::always_inline]] inline
  Smword
  mfc0_32(unsigned reg, unsigned sel)
  {
    if (!is_32bit(reg, sel))
      FIASCO_CP0_SIZE_ERROR("32bit", reg, sel);

    Smword v;
    asm volatile (
        "mfc0\t%0, $%1, %2\n\t"
        : "=r"(v) : "i"(reg), "i"(sel));
    return v;
  }

  [[gnu::always_inline]] inline
  void
  mfgc0_32(Mword *v, unsigned reg, unsigned sel)
  { *v = mfgc0_32(reg, sel); }

  [[gnu::always_inline]] inline
  void
  mtgc0_32(Unsigned32 v, unsigned reg, unsigned sel)
  {
    if (!is_32bit(reg, sel))
      FIASCO_CP0_SIZE_ERROR("32bit", reg, sel);

    asm (".set push\n\t.set virt\n\t"
         "mtgc0\t%z0, $%1, %2\n\t"
         ".set pop" : : "Jr"(v), "i"(reg), "i"(sel));
  }

  [[gnu::always_inline]] inline
  void
  mtc0_32(Unsigned32 v, unsigned reg, unsigned sel)
  {
    if (!is_32bit(reg, sel))
      FIASCO_CP0_SIZE_ERROR("32bit", reg, sel);

    asm ("mtc0\t%z0, $%1, %2\n\t"
         : : "Jr"(v), "i"(reg), "i"(sel));
  }

  inline void ehb()
  { __asm__ __volatile__ ("ehb"); }

  inline void synci(void const *addr)
  { __asm__ __volatile__ ("synci 0(%0)" : : "r"(addr) : "memory"); }

  inline void tlbr()
  { __asm__ __volatile__ ("tlbr"); }

  inline void tlbwr()
  { __asm__ __volatile__ ("tlbwr"); }

  inline void tlbwi()
  { __asm__ __volatile__ ("tlbwi"); }

  inline void tlbinv()
  { __asm__ __volatile__ (".set push; .set eva; tlbinv; .set pop"); }

  inline void tlbinvf()
  { __asm__ __volatile__ (".set push; .set eva; tlbinvf; .set pop"); }

#if defined(CONFIG_BIT32)

[[gnu::always_inline]] inline
void
mfgc0(Mword *v, unsigned reg, unsigned sel)
{
  if (!is_mword(reg, sel))
    FIASCO_CP0_SIZE_ERROR("mword", reg, sel);

  // note: the "m" constraint is to force the compiler to assume
  // that the asm statement has varying inputs and might not be
  // moved out of loops etc.
  asm (".set push\n\t.set virt\n\t"
       "mfgc0\t%0, $%1, %2\n\t"
       ".set pop"
       : "=r"(*v) : "i"(reg), "i"(sel), "m"(*v));
}

[[gnu::always_inline]] inline
Smword
mfgc0(unsigned reg, unsigned sel)
{
  if (!is_mword(reg, sel))
    FIASCO_CP0_SIZE_ERROR("mword", reg, sel);

  Smword v;
  asm volatile (
      ".set push\n\t.set virt\n\t"
      "mfgc0\t%0, $%1, %2\n\t"
      ".set pop"
      : "=r"(v) : "i"(reg), "i"(sel));
  return v;
}

[[gnu::always_inline]] inline
Mword
mfc0(unsigned reg, unsigned sel)
{
  if (!is_mword(reg, sel))
    FIASCO_CP0_SIZE_ERROR("mword", reg, sel);

  Mword v;
  asm volatile (
      "mfc0\t%0, $%1, %2\n\t"
      : "=r"(v) : "i"(reg), "i"(sel));
  return v;
}

[[gnu::always_inline]] inline
void
mtgc0(Mword v, unsigned reg, unsigned sel)
{
  if (!is_mword(reg, sel))
    FIASCO_CP0_SIZE_ERROR("mword", reg, sel);

  asm (".set push\n\t.set virt\n\t"
       "mtgc0\t%z0, $%1, %2\n\t"
       ".set pop" : : "Jr"(v), "i"(reg), "i"(sel));
}

[[gnu::always_inline]] inline
void
mtc0(Mword v, unsigned reg, unsigned sel)
{
  if (!is_mword(reg, sel))
    FIASCO_CP0_SIZE_ERROR("mword", reg, sel);

  asm ("mtc0\t%z0, $%1, %2\n\t"
       : : "Jr"(v), "i"(reg), "i"(sel));
}

#endif // CONFIG_BIT32

#if defined (CONFIG_BIT64)
[[gnu::always_inline]] inline
void
mfgc0(Mword *v, unsigned reg, unsigned sel)
{
  if (!is_mword(reg, sel))
    FIASCO_CP0_SIZE_ERROR("mword", reg, sel);

  // note: the "m" constraint is to force the compiler to assume
  // that the asm statement has varying inputs and might not be
  // moved out of loops etc.
  asm (".set push\n\t.set virt\n\t"
       "dmfgc0\t%0, $%1, %2\n\t"
       ".set pop"
       : "=r"(*v) : "i"(reg), "i"(sel), "m"(*v));
}

[[gnu::always_inline]] inline
Smword
mfgc0(unsigned reg, unsigned sel)
{
  if (!is_mword(reg, sel))
    FIASCO_CP0_SIZE_ERROR("mword", reg, sel);

  Smword v;
  asm volatile (
      ".set push\n\t.set virt\n\t"
      "dmfgc0\t%0, $%1, %2\n\t"
      ".set pop"
      : "=r"(v) : "i"(reg), "i"(sel));
  return v;
}

[[gnu::always_inline]] inline
Mword
mfc0(unsigned reg, unsigned sel)
{
  if (!is_mword(reg, sel))
    FIASCO_CP0_SIZE_ERROR("mword", reg, sel);

  Mword v;
  asm volatile (
      "dmfc0\t%0, $%1, %2\n\t"
      : "=r"(v) : "i"(reg), "i"(sel));
  return v;
}

[[gnu::always_inline]] inline
void
mtgc0(Mword v, unsigned reg, unsigned sel)
{
  if (!is_mword(reg, sel))
    FIASCO_CP0_SIZE_ERROR("mword", reg, sel);

  asm (".set push\n\t.set virt\n\t"
       "dmtgc0\t%z0, $%1, %2\n\t"
       ".set pop" : : "Jr"(v), "i"(reg), "i"(sel));
}

[[gnu::always_inline]] inline
void
mtc0(Mword v, unsigned reg, unsigned sel)
{
  if (!is_mword(reg, sel))
    FIASCO_CP0_SIZE_ERROR("mword", reg, sel);

  asm ("dmtc0\t%z0, $%1, %2\n\t"
       : : "Jr"(v), "i"(reg), "i"(sel));
}
#endif // CONFIG_BIT64

[[gnu::always_inline]] inline
void mfgc0(Mword *v, Cp0_regs r) { mfgc0(v, r >> 3, r & 7); }

[[gnu::always_inline]] inline
Smword mfgc0(Cp0_regs r) { return mfgc0(r >> 3, r & 7); }

[[gnu::always_inline]] inline
Mword mfc0(Cp0_regs r) { return mfc0(r >> 3, r & 7); }

[[gnu::always_inline]] inline
Smword mfgc0_32(Cp0_regs r) { return mfgc0_32(r >> 3, r & 7); }

[[gnu::always_inline]] inline
Smword mfc0_32(Cp0_regs r) { return mfc0_32(r >> 3, r & 7); }

[[gnu::always_inline]] inline
void mfgc0_32(Mword *v, Cp0_regs r) { *v = mfgc0_32(r >> 3, r & 7); }


[[gnu::always_inline]] inline
void mtgc0(Mword v, Cp0_regs r) { mtgc0(v, r >> 3, r & 7); }

[[gnu::always_inline]] inline
void mtc0(Mword v, Cp0_regs r) { mtc0(v, r >> 3, r & 7); }

[[gnu::always_inline]] inline
void mtgc0_32(Unsigned32 v, Cp0_regs r) { mtgc0_32(v, r >> 3, r & 7); }

[[gnu::always_inline]] inline
void mtc0_32(Unsigned32 v, Cp0_regs r) { mtc0_32(v, r >> 3, r & 7); }

} // namespace Mips

