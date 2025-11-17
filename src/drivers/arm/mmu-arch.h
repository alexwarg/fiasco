#pragma once

#include <mmu-arm-bits.h>
#include <mem.h>
#include <globalconfig.h>

template< typename GEN, unsigned long Flush_area = 0, bool Ram = false >
class Mmu_arch : public Mmu_arm_bits<GEN, Flush_area, Ram>
{
private:
  using B = Mmu_arm_bits<GEN, Flush_area, Ram>;
#if defined(CONFIG_ARM_V7PLUS)
  template<typename T>
  static FIASCO_NO_UNROLL_LOOPS void __attribute__((always_inline))
  set_way_full_loop(T const &f)
  {
    Mem::dmb();
    Unsigned64 clidr = B::get_clidr();
    // Level of Coherency CLIDR[26:24] * 2 to simplify
    //   get_ccsidr((cache level << 1) | 0)
    unsigned lvl = ((clidr >> 24) & 7) << 1;

    unsigned assoc_mask;
    unsigned set_shift;
    unsigned set_mask;
    if (B::has_feat_ccidx())
      {
        assoc_mask = 0x1fffffff;
        set_shift = 32;
        set_mask = 0xffffffff;
      }
    else
      {
        assoc_mask = 0x3ff;
        set_shift = 13;
        set_mask = 0x7fff;
      }


    for (unsigned cl = 0; cl < lvl; cl += 2, clidr >>= 3)
      {
        // - 0x2 data cache only
        // - 0x3 separate instruction/data caches
        // - 0x4 unified cache
        if ((clidr & 6) == 0)
          continue;

        Mword ccsidr = B::get_ccsidr(cl);

        unsigned assoc       = ((ccsidr >> 3) & assoc_mask);
        unsigned w_shift     = __builtin_clz(assoc);
        unsigned set         = ((ccsidr >> set_shift) & set_mask);
        unsigned log2linelen = (ccsidr & 7) + 4;
        do
          {
            unsigned w = assoc;
            do
              f((w << w_shift) | (set << log2linelen) | cl);
            while (w--);
          }
        while (set--);
      }

    B::btc_inv();
    Mem::dsb();
    Mem::isb();
  }

public:
  using B::flush_dcache;
  static void flush_dcache()
  {
    Mem::dsb();
    set_way_full_loop(B::dc_cisw);
  }

  using B::flush_cache;
  static void flush_cache()
  {
    Mem::dsb();
    B::ic_iallu();
    set_way_full_loop(B::dc_cisw);
  }

  using B::clean_dcache;
  static void clean_dcache()
  {
    Mem::dsb();
    set_way_full_loop(B::dc_csw);
  }
#endif // CONFIG_ARM_V7 || CONFIG_ARM_V8
};
