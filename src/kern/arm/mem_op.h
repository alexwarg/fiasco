#pragma once

#include "globalconfig.h"
#include "mem.h"
#include "mem_unit.h"
#include "outer_cache.h"
#include "types.h"

struct Mem_op_outer_cache_none
{
  template<typename FUNC>
  static inline void outer_cache_op(Address, Address, FUNC &&) {}
};

#ifdef CONFIG_ARM_OUTER_CACHE

#include "context.h"
#include "mem_space.h"
#include "space.h"

struct Mem_op_outer_cache_l2
{
  template<typename FUNC>
  static void outer_cache_op(Address start, Address end, FUNC &&f)
  {
    Virt_addr v = Virt_addr(start);
    Virt_addr e = Virt_addr(end);

    Context *c = current();

    while (v < e)
      {
        Mem_space::Page_order phys_size;
        Mem_space::Phys_addr phys_addr;
        Page::Attr attrs;
        bool mapped = (   c->mem_space()->v_lookup(Mem_space::Vaddr(v), &phys_addr, &phys_size, &attrs)
                       && (attrs.rights & Page::Rights::U()));

        Virt_size sz = Virt_size(1) << phys_size;
        Virt_size offs = cxx::get_lsb(v, phys_size);
        sz -= offs;
        if (e - v < sz)
          sz = e - v;

        if (mapped)
          {
            Virt_addr vstart = Virt_addr(phys_addr) | offs;
            Virt_addr vend = vstart + sz;
            f(cxx::int_value<Virt_addr>(vstart), cxx::int_value<Virt_addr>(vend), false);
          }
        v += sz;
      }
    Outer_cache::sync();
  }
};

using Mem_op_outer_cache_base = Mem_op_outer_cache_l2;

#else // !CONFIG_ARM_OUTER_CACHE

using Mem_op_outer_cache_base = Mem_op_outer_cache_none;

#endif // CONFIG_ARM_OUTER_CACHE

#if defined(CONFIG_CPU_VIRT) && !defined(CONFIG_ARM_OUTER_CACHE)
// context.h / mem_space.h / space.h already included above when outer cache
// is enabled; include them here for the cpu_virt-only case.
#include "context.h"
#include "mem_space.h"
#include "space.h"
#endif

class Mem_op : private Mem_op_outer_cache_base
{
  using Mem_op_outer_cache_base::outer_cache_op;

public:
  enum Op_cache
  {
    Op_cache_clean_data        = 0x00,
    Op_cache_flush_data        = 0x01,
    Op_cache_inv_data          = 0x02,
    Op_cache_coherent          = 0x03,
    Op_cache_dma_coherent      = 0x04,
    Op_cache_dma_coherent_full = 0x05,
  };

  enum Op_mem
  {
    Op_mem_read_data     = 0x10,
    Op_mem_write_data    = 0x11,
  };

private:
  // Arch-specific — defined in 32/mem_op-arm-32.cc or 64/mem_op-arm-64.cc
  static void inv_icache(Address start, Address end);

  static void l1_inv_dcache(Address start, Address end);

  static inline void
  __arm_kmem_cache_maint(int op, void const *kstart, void const *kend)
  {
    switch (op)
      {
      case Op_cache_clean_data:
        Mem_unit::clean_dcache(kstart, kend);
        Mem::barrier();
        outer_cache_op(Address(kstart), Address(kend),
                       [](Address s, Address e, bool sync)
                       { Outer_cache::clean(s, e, sync); });
        break;

      case Op_cache_flush_data:
      case Op_cache_inv_data:
        Mem_unit::flush_dcache(kstart, kend);
        Mem::barrier();
        outer_cache_op(Address(kstart), Address(kend),
                       [](Address s, Address e, bool sync)
                       { Outer_cache::flush(s, e, sync); });
        break;

      case Op_cache_coherent:
        Mem_unit::clean_dcache(kstart, kend);
        // Our outer cache model assumes a unified outer cache, so there is no
        // need to clean it in order to achieve cache coherency
        Mem::dsb();
        Mem_unit::btc_inv();
        inv_icache(Address(kstart), Address(kend));
        Mem::dsb();
        break;

      case Op_cache_dma_coherent:
        Mem_unit::flush_dcache(kstart, kend);
        Mem::barrier();
        outer_cache_op(Address(kstart), Address(kend),
                       [](Address s, Address e, bool sync)
                       { Outer_cache::flush(s, e, sync); });
        break;

      // We might not want to implement this one but single address outer
      // cache flushing can be really slow
      case Op_cache_dma_coherent_full:
        Mem_unit::flush_dcache();
        Mem::barrier();
        Outer_cache::flush();
        break;

      default:
        break;
      };
  }

#ifdef CONFIG_CPU_VIRT
  static inline void
  __arm_mem_cache_maint(int op, void const *start, void const *end)
  {
    if (op == Op_cache_dma_coherent_full)
      {
        __arm_kmem_cache_maint(Op_cache_dma_coherent_full, 0, 0);
        return;
      }

    Virt_addr v = Virt_addr((Address)start);
    Virt_addr e = Virt_addr((Address)end);

    Context *c = current();

    while (v < e)
      {
        Mem_space::Page_order phys_size;
        Mem_space::Phys_addr phys_addr;
        Page::Attr attrs;
        bool mapped = (   c->mem_space()->v_lookup(Mem_space::Vaddr(v), &phys_addr, &phys_size, &attrs)
                       && (attrs.rights & Page::Rights::U()));

        Virt_size sz = Virt_size(1) << phys_size;
        Virt_size offs = cxx::get_lsb(v, phys_size);
        sz -= offs;
        if (e - v < sz)
          sz = e - v;

        if (mapped)
          {
            Virt_addr vstart = Virt_addr(phys_addr) | offs;
            Virt_addr vend = vstart + sz;
            __arm_kmem_cache_maint(op, (void *)vstart, (void *)vend);
          }
        v += sz;
      }
  }
#else // !CONFIG_CPU_VIRT
  static inline void
  __arm_mem_cache_maint(int op, void const *start, void const *end)
  { __arm_kmem_cache_maint(op, start, end); }
#endif // CONFIG_CPU_VIRT

public:
  static void arm_mem_cache_maint(int op, void const *start, void const *end);

#ifndef CONFIG_CPU_VIRT
  static void arm_mem_access(Mword *r);
#endif
};
