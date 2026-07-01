
#include "jdb_ptab.h"

#include "paging.h"
#include "simpleio.h"
#include <stdio.h>
#include <jdb_core.h>

// -----------------------------------------------------------------------
// ARM non-LPAE print_entry helpers and implementation

#ifndef CONFIG_ARM_LPAE

#if defined(CONFIG_ARM_V5)

static inline bool
jdb_ptab_arm_is_cached(Pdir::Pte_ptr const &entry)
{
  if (entry.level == 0 && (*entry.pte & 3) != 2)
    return true; /* No caching options on PDEs */
  return (*entry.pte & Page::Cache_mask) == Page::CACHEABLE;
}

static inline bool
jdb_ptab_arm_is_executable(Pdir::Pte_ptr const &)
{
  return 1;
}

static inline char
jdb_ptab_arm_ap_char(unsigned ap)
{
  return ap & 0x2 ? (ap & 0x1) ? 'w' : 'r'
                  : (ap & 0x1) ? 'W' : 'R';
}

#else // arm_v6plus, !arm_lpae

static inline bool
jdb_ptab_arm_is_cached(Pdir::Pte_ptr const &entry)
{
  if (entry.level == Pdir::root_level())
    {
      if ((*entry.pte & 3) == 2)
        return (*entry.pte & Page::Section_cache_mask) == Page::Section_cachable_bits;
      return true;
    }

  return (*entry.pte & Page::Cache_mask) == Page::CACHEABLE;
}

static inline bool
jdb_ptab_arm_is_executable(Pdir::Pte_ptr const &entry)
{
  return (*entry.pte & 3) == 2 || (*entry.pte & 3) == 1;
}

static inline char
jdb_ptab_arm_ap_char(unsigned ap)
{
  switch (ap & 0x23)
  {
    case    0: return '-';
    case    1: return 'W';
    case    2: return 'r';
    case    3: return 'w';
    case 0x21: return 'R';
    case 0x22: case 0x23: return 'r';
    default: return '?';
  };
}

#endif // arm_v5 / arm_v6plus

void
Jdb_ptab::print_entry(Pdir::Pte_ptr const &entry)
{
  if (dump_raw)
    printf("%08x", *entry.pte);
  else
    {
      if (!entry.is_valid())
        {
          putstr("    -   ");
          return;
        }
      Address phys = entry_phys(entry);

      unsigned t = *entry.pte & 0x03;
      unsigned ap = *entry.pte >> 4;
      char ps;
      if (entry.level == Pdir::root_level())
        switch (t)
          {
          case 1: ps = 'C'; break;
          case 2: ps = 'S'; ap = *entry.pte >> 10; break;
          case 3: ps = 'F'; break;
          default: ps = 'U'; break;
          }
      else
        switch (t)
          {
          case 1: ps = 'l'; break;
          case 2: ps = 's'; break;
          case 3: ps = 't'; break;
          default: ps = 'u'; break;
          }

      printf("%05lx%s%c", phys >> Config::PAGE_SHIFT,
                          jdb_ptab_arm_is_cached(entry)
                           ? "-" : JDB_ANSI_COLOR(lightblue) "n" JDB_ANSI_END,
                          ps);
      if (entry.level == Pdir::root_level() && t != 2)
        putchar('-');
      else
        printf("%s%c" JDB_ANSI_END,
               jdb_ptab_arm_is_executable(entry) ? "" : JDB_ANSI_COLOR(red),
               jdb_ptab_arm_ap_char(ap));
    }
}

#else // CONFIG_ARM_LPAE

static inline bool
jdb_ptab_arm_is_cached(Pdir::Pte_ptr const &entry)
{
  if (!entry.is_leaf())
    return true;
  return (*entry.pte & Page::Cache_mask) == Page::CACHEABLE;
}

static inline bool
jdb_ptab_arm_is_executable(Pdir::Pte_ptr const &entry)
{
  return !(*entry.pte & 0x0040000000000000);
}

#ifdef CONFIG_CPU_VIRT

static inline char
jdb_ptab_arm_ap_char(Pdir::Pte_ptr const &entry)
{
  return ((*entry.pte >> 7) & 1) ? 'w' : 'r';
}

#else

static inline char
jdb_ptab_arm_ap_char(Pdir::Pte_ptr const &entry)
{
  static char const ch[] = { 'W', 'w', 'R', 'r' };
  return ch[(*entry.pte >> 6) & 3];
}

#endif // CONFIG_CPU_VIRT

void
Jdb_ptab::print_entry(Pdir::Pte_ptr const &entry)
{
  if (dump_raw)
    printf("%16llx", *entry.pte);
  else
    {
      if (!entry.is_valid())
        {
          putstr("        -       ");
          return;
        }
      Address phys = entry_phys(entry);

      unsigned t = (*entry.pte & 2) >> 1;

      char ps;
      if (entry.level == Pdir::root_level())
        switch (t)
          {
          case 0: ps = 'G'; break;
          case 1: ps = 'P'; break;
          default: ps = '?'; break;
          }
      else if (entry.level == Pdir::from_root_level(1))
        switch (t)
          {
          case 0: ps = 'M'; break;
          case 1: ps = 'P'; break;
          default: ps = '?'; break;
          }
      else
        switch (t)
          {
          case 0: ps = '?'; break;
          case 1: ps = 'p'; break;
          default: ps = '?'; break;
          }

      printf("%13lx%s%c", phys >> Config::PAGE_SHIFT,
                          jdb_ptab_arm_is_cached(entry)
                           ? "-" : JDB_ANSI_COLOR(lightblue) "n" JDB_ANSI_END,
                          ps);
      if (!entry.is_leaf())
        putchar('-');
      else
        printf("%s%c" JDB_ANSI_END,
               jdb_ptab_arm_is_executable(entry) ? "" : JDB_ANSI_COLOR(red),
               jdb_ptab_arm_ap_char(entry));
    }
}

#endif // CONFIG_ARM_LPAE
