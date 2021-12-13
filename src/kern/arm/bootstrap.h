#pragma once

/**
 * Helper to apply REL(A) relocations at startup.
 *
 * It requires that relocations are combined in a single section and that only
 * relative relocations exist.
 *
 * \param DYN   The dynamic section entry type
 * \param RELOC The sole relocation entry type (REL or RELA)
 */
template<typename DYN, typename RELOC>
struct Elf
{
  static inline unsigned long
  elf_dynamic_link_addr()
  {
    extern unsigned long const _GLOBAL_OFFSET_TABLE_[]
      __attribute__ ((visibility ("hidden")));
    return _GLOBAL_OFFSET_TABLE_[0];
  }

  static inline unsigned long
  elf_dynamic_section()
  {
    extern char _DYNAMIC[] __attribute__ ((visibility ("hidden")));
    return (unsigned long)&_DYNAMIC[0];
  }

  static inline unsigned long
  elf_load_address()
  {
    return elf_dynamic_section() - elf_dynamic_link_addr();
  }

  static inline unsigned long
  relocate()
  {
    unsigned long load_addr = elf_load_address();
    DYN *dyn = (DYN *)elf_dynamic_section();
    unsigned long relcnt = 0;
    RELOC *rel = 0;

    if (!load_addr)
      return 0;

    for (int i = 0; dyn[i].tag != 0; i++)
      switch (dyn[i].tag)
        {
        case DYN::Reloc:       rel = (RELOC*)(dyn[i].ptr + load_addr); break;
        case DYN::Reloc_count: relcnt = dyn[i].val; break;
        }

    if (!rel || !relcnt)
      return load_addr;

    for (; relcnt; relcnt--, rel++)
      rel->apply(load_addr);

    return load_addr;
  }
};


