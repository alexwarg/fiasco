#pragma once

#include <config_gdt.h>
#include <types.h>
#include <x86desc.h>

#include <globalconfig.h>

class Gdt
{
public:
   /** Segment numbers. */
  enum
  {
    gdt_tss             = GDT_TSS,
    gdt_code_kernel     = GDT_CODE_KERNEL,
    gdt_data_kernel     = GDT_DATA_KERNEL,
    gdt_code_user       = GDT_CODE_USER,
    gdt_data_user       = GDT_DATA_USER,
    gdt_tss_dbf         = GDT_TSS_DBF,
    gdt_utcb            = GDT_UTCB,
    gdt_ldt             = GDT_LDT,
    gdt_user_entry1     = GDT_USER_ENTRY1,
    gdt_user_entry2     = GDT_USER_ENTRY2,
    gdt_user_entry3     = GDT_USER_ENTRY3,
    gdt_user_entry4     = GDT_USER_ENTRY4,
    gdt_code_user32     = GDT_CODE_USER32,
    gdt_max             = GDT_MAX,
  };

  enum
  {
    Selector_user       = 0x03,
    Selector_kernel     = 0x00,
  };

#ifdef CONFIG_BIT32
  void set_entry_tss(unsigned nr, Address base, Unsigned32 limit)
  {
    _entries[nr] = Gdt_entry(base, limit, Gdt_entry::Tss_available,
                             Gdt_entry::Kernel, Gdt_entry::Granularity_bytes);
  }
#endif
#ifdef CONFIG_BIT64
  void set_entry_tss(unsigned nr, Address base, Unsigned32 limit)
  {
    _entries[nr] = Gdt_entry(base, limit >> 12, Gdt_entry::Tss_available,
                             Gdt_entry::Kernel, Gdt_entry::Granularity_4k);
    _entries[nr + 1] = Gdt_entry(base);
  }
#endif

  explicit Gdt(unsigned nr_entries = Gdt::gdt_max / sizeof(Gdt_entry))
  {
    for (unsigned i = 0; i < nr_entries; ++i)
      _entries[i] = Gdt_entry();
  }

  void set_entry_4k(unsigned nr, Address base, Unsigned32 limit,
                    Gdt_entry::Access access, Gdt_entry::Type type,
                    Gdt_entry::Dpl dpl, Gdt_entry::Code code,
                    Gdt_entry::Default_size default_size)
  {
    _entries[nr] = Gdt_entry(base, limit >> 12, access, type, dpl, code,
                             default_size, Gdt_entry::Granularity_4k);
  }

  void set_entry_ldt(unsigned nr, Address base, Unsigned32 limit)
  {
    _entries[nr] = Gdt_entry(base, limit, Gdt_entry::Ldt, Gdt_entry::Kernel,
                             Gdt_entry::Granularity_bytes);
  }

  void clear_entry(unsigned nr)
  {
    _entries[nr] = Gdt_entry();
  }

  Gdt_entry *entries()
  {
    return _entries;
  }

  Gdt_entry &operator [](unsigned idx)
  {
    return _entries[idx];
  }

  Gdt_entry const &operator [](unsigned idx) const
  {
    return _entries[idx];
  }

  static void set(Pseudo_descriptor *desc)
  {
    asm volatile ("lgdt %0" : : "m" (*desc));
  }

  static void get(Pseudo_descriptor *desc)
  {
    asm volatile ("sgdt %0" : "=m" (*desc) : : "memory");
  }

  static int data_segment()
  {
    return gdt_data_user | Selector_user;
  }

private:
  Gdt_entry _entries[0];
};

