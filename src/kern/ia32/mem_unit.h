#pragma once

#include "types.h"
#include <globalconfig.h>

class Mem_unit
{
public:
  enum { Asid_invalid = 1 << 12 };

#ifdef CONFIG_IA32_PCID
private:
  /** INVPCID types */
  enum
  {
    Invpcid_single_address      = 0, /**< Individual address, except global translations */
    Invpcid_single_context      = 1, /**< Single-context, except global translations */
    Invpcid_all_context_global  = 2, /**< All-context, including globals */
    Invpcid_all_context_non_global = 3, /**< All-context except global translations */
  };

  /**
   * Flush the TLB, either at a virtual address or flush all mappings associated
   * with a PCID.
   */
  static inline ALWAYS_INLINE void
  _invalidate_pcid(unsigned pcid, Address address, unsigned type)
  {
    struct
    {
      unsigned long pcid, address;
    } descriptor = { pcid, address };
    __asm__ __volatile__ ("invpcid %0, %1\n" : : "m" (descriptor),
                                               "r" ((unsigned long)type)
                                             : "memory");
  }

public:
#endif // CONFIG_IA32_PCID

  static inline ALWAYS_INLINE void
  make_coherent_to_pou(void const *, size_t)
  {}

  static inline ALWAYS_INLINE void
  clean_dcache()
  { asm volatile ("wbinvd"); }

  static inline ALWAYS_INLINE void
  clean_dcache(void const *addr)
  { asm volatile ("clflush %0" : : "m" (*(char const *)addr)); }

  static inline ALWAYS_INLINE void
  clean_dcache(void const *start, void const *end)
  {
    enum { Cl_size = 64 };
    if (((Address)end) - ((Address)start) >= 8192)
      clean_dcache();
    else
      for (char const *s = (char const *)start; s < (char const *)end;
           s += Cl_size)
        clean_dcache(s);
  }

#ifndef CONFIG_IA32_PCID

  /** Flush the whole TLB. */
  static inline ALWAYS_INLINE void
  tlb_flush()
  {
    Mword dummy;
    __asm__ __volatile__ ("mov %%cr3,%0; mov %0,%%cr3 " : "=r"(dummy) : : "memory");
  }

  /** Flush the whole TLB during early boot when PCID is not yet enabled. */
  static inline ALWAYS_INLINE void
  tlb_flush_early()
  { tlb_flush(); }

  /** Flush TLB at virtual address. */
  static inline ALWAYS_INLINE void
  tlb_flush(Address addr)
  { __asm__ __volatile__ ("invlpg %0" : : "m" (*(char*)addr) : "memory"); }

  static inline ALWAYS_INLINE void
  tlb_flush_kernel(Address addr)
  { return tlb_flush(addr); }

#else // CONFIG_IA32_PCID

  /** Flush the whole TLB. */
  static inline ALWAYS_INLINE void
  tlb_flush()
  { _invalidate_pcid(0, 0, Invpcid_all_context_global); }

  /** Flush the whole TLB during early boot when PCID is not yet enabled. */
  static inline ALWAYS_INLINE void
  tlb_flush_early()
  {
    Mword dummy;
    __asm__ __volatile__ ("mov %%cr3,%0; mov %0,%%cr3 " : "=r"(dummy) : : "memory");
  }

  /** Flush the whole TLB for the given PCID. */
  static inline ALWAYS_INLINE void
  tlb_flush(unsigned pcid)
  { _invalidate_pcid(pcid, 0, Invpcid_single_context); }

  /** Flush TLB at virtual address of the given PCID. */
  static inline ALWAYS_INLINE void
  tlb_flush(unsigned pcid, Address addr)
  { _invalidate_pcid(pcid, addr, Invpcid_single_address); }

  /** Flush the whole TLB for the kernel PCID 0. */
  static inline ALWAYS_INLINE void
  tlb_flush_kernel()
  { _invalidate_pcid(0, 0, Invpcid_single_context); }

  /** Flush TLB at virtual address. */
  static inline ALWAYS_INLINE void
  tlb_flush_kernel(Address addr)
  { _invalidate_pcid(0, addr, Invpcid_single_address); }

#endif // CONFIG_IA32_PCID
};
