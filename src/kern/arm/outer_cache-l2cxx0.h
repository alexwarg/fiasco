#pragma once

#include <mmio_register_block.h>
#include <spin_lock.h>
#include <lock_guard.h>
#include <processor.h>

namespace Outer_cache
{
  namespace Priv
  {
    class L2cxx0 : public Mmio_register_block
    {
    public:
      enum
      {
        CACHE_ID                       = 0x000,
        CACHE_TYPE                     = 0x004,
        CONTROL                        = 0x100,
        AUX_CONTROL                    = 0x104,
        TAG_RAM_CONTROL                = 0x108,
        DATA_RAM_CONTROL               = 0x10c,
        EVENT_COUNTER_CONTROL          = 0x200,
        EVENT_COUTNER1_CONFIG          = 0x204,
        EVENT_COUNTER0_CONFIG          = 0x208,
        EVENT_COUNTER1_VALUE           = 0x20c,
        EVENT_COUNTER0_VALUE           = 0x210,
        INTERRUPT_MASK                 = 0x214,
        MASKED_INTERRUPT_STATUS        = 0x218,
        RAW_INTERRUPT_STATUS           = 0x21c,
        INTERRUPT_CLEAR                = 0x220,
        CACHE_SYNC                     = 0x730,
        INVALIDATE_LINE_BY_PA          = 0x770,
        INVALIDATE_BY_WAY              = 0x77c,
        CLEAN_LINE_BY_PA               = 0x7b0,
        CLEAN_LINE_BY_INDEXWAY         = 0x7bb,
        CLEAN_BY_WAY                   = 0x7bc,
        CLEAN_AND_INV_LINE_BY_PA       = 0x7f0,
        CLEAN_AND_INV_LINE_BY_INDEXWAY = 0x7f8,
        CLEAN_AND_INV_BY_WAY           = 0x7fc,
        LOCKDOWN_BY_WAY_D_SIDE         = 0x900,
        LOCKDOWN_BY_WAY_I_SIDE         = 0x904,
        TEST_OPERATION                 = 0xf00,
        LINE_TAG                       = 0xf30,
        DEBUG_CONTROL_REGISTER         = 0xf40,
      };

      enum
      {
        Pwr_ctrl_standby_mode_en       = 1 << 0,
        Pwr_ctrl_dynamic_clk_gating_en = 1 << 1,
      };

      Spin_lock<> _lock;

      explicit L2cxx0(void *virt) : Mmio_register_block(virt) {}

      void write_op(Address reg, Mword val)
      {
        Mmio_register_block::write<Mword>(val, reg);
        while (read<Mword>(reg) & 1)
          ;
      }

      void write_way_op(Address reg, Mword val)
      {
        Mmio_register_block::write<Mword>(val, reg);
        while (read<Mword>(reg) & val)
          ;
      }
    };

    extern Static_object<L2cxx0> l2cxx0;
    extern bool need_sync;
    extern unsigned waymask;
  } // namesapce Priv

  enum
  {
    Cache_line_shift = 5,
    Cache_line_size = 1 << Cache_line_shift,
    Cache_line_mask = Cache_line_size - 1,
  };

  inline void sync()
  {
    using namespace Priv;
    while (l2cxx0->read<Mword>(L2cxx0::CACHE_SYNC))
      Proc::pause(); //preemption_point();
  }

  inline void clean()
  {
    using namespace Priv;
    auto guard = lock_guard(l2cxx0->_lock);
    l2cxx0->write_way_op(L2cxx0::CLEAN_BY_WAY, waymask);
    sync();
  }

  inline void clean(Mword phys_addr, bool do_sync = true)
  {
    using namespace Priv;
    auto guard = lock_guard(l2cxx0->_lock);
    l2cxx0->write_op(L2cxx0::CLEAN_LINE_BY_PA, phys_addr & (~0UL << Cache_line_shift));
    if (need_sync && do_sync)
      sync();
  }

  inline void flush()
  {
    using namespace Priv;
    auto guard = lock_guard(l2cxx0->_lock);
    l2cxx0->write_way_op(L2cxx0::CLEAN_AND_INV_BY_WAY, waymask);
    sync();
  }

  inline void flush(Mword phys_addr, bool do_sync = true)
  {
    using namespace Priv;
    auto guard = lock_guard(l2cxx0->_lock);
    l2cxx0->write_op(L2cxx0::CLEAN_AND_INV_LINE_BY_PA, phys_addr & (~0UL << Cache_line_shift));
    if (need_sync && do_sync)
      sync();
  }

  inline void invalidate()
  {
    using namespace Priv;
    auto guard = lock_guard(l2cxx0->_lock);
    l2cxx0->write_way_op(L2cxx0::INVALIDATE_BY_WAY, waymask);
    sync();
  }

  inline void invalidate(Address phys_addr, bool do_sync = true)
  {
    using namespace Priv;
    auto guard = lock_guard(l2cxx0->_lock);
    l2cxx0->write_op(L2cxx0::INVALIDATE_LINE_BY_PA, phys_addr & (~0UL << Cache_line_shift));
    if (need_sync && do_sync)
      sync();
  }
} // namespace Outer_cache
