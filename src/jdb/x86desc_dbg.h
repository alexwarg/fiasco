#pragma once

#include <x86desc.h>
#include <globalconfig.h>
#include <std_macros.h>
#include <cstdio>

namespace Dbg
{

static char const *
desc_size_str(Gdt_entry const &e)
{
  if (e.code() == Gdt_entry::Code_64bit)
    return "64-bit";

  if (e.default_size() == Gdt_entry::Size_32)
    return "32-bit";

  return "16-bit";
}

/**
 * Get description of system segment type on IA-32.
 *
 * \param type_system  Segment type.
 *
 * \return Textual description of system segment type on IA-32.
 */
static char const *
desc_type_system_str32(X86desc const &e)
{
  static char const *const desc_type_system32[16] =
    {
      "reserved",
      "16-bit TSS (available)",
      "LDT",
      "16-bit TSS (busy)",
      "16-bit call gate",
      "task gate",
      "16-bit intr gate",
      "16-bit trap gate",
      "reserved",
      "32-bit TSS (available)",
      "reserved",
      "32-bit TSS (busy)",
      "32-bit call gate",
      "reserved",
      "32-bit intr gate",
      "32-bit trap gate"
    };

  return desc_type_system32[e.type_system()];
}

/**
 * Get description of system segment type on AMD64.
 *
 * \param type_system  Segment type.
 *
 * \return Textual description of system segment type on AMD64.
 */
static char const *
desc_type_system_str64(X86desc const &e)
{
  static char const *const desc_type_system64[16] =
    {
      "reserved",
      "reserved",
      "LDT",
      "reserved",
      "reserved",
      "reserved",
      "reserved",
      "reserved",
      "reserved",
      "64-bit TSS (available)",
      "reserved",
      "64-bit TSS (busy)",
      "64-bit call gate",
      "reserved",
      "64-bit intr gate",
      "64-bit trap gate"
    };

  return desc_type_system64[e.type_system()];
}


/**
 * Get description of system segment type.
 *
 * \param type_system  Segment type.
 *
 * \return Textual description of system segment type.
 */
static char const *
desc_type_system_str(X86desc const &e)
{
  if (IS_ENABLED(CONFIG_BIT64))
    return desc_type_system_str64(e);

  return desc_type_system_str32(e);
}

/**
 * Get description of non-system segment type.
 *
 * \param type  Segment type.
 *
 * \return Textual description of non-system segment type.
 */
static char const *
desc_type_str(Gdt_entry const &e)
{
  static char const *const desc_type[8] =
    {
      "data r-o",
      "data r/w",
      "data r-o exp-dn",
      "data r/w exp-dn",
      "code x-o",
      "code x/r",
      "code x-o conf",
      "code x/r conf"
    };

  return desc_type[e.type()];
}


/**
 * Print human-readable segment descriptor contents.
 */
inline static void
desc_show(Gdt_entry const &e)
{
  if (e.system() && IS_ENABLED(CONFIG_BIT64))
    {
      // 64-bit system descriptor (one entry occupies 16 bytes)

      switch (e.type_system())
        {
        case Gdt_entry::Tss_available:
        case Gdt_entry::Tss_busy:
          printf("%08lx-%08lx", e.base(), e.base() + e.size());
          break;
        default:
          // Unknown system descriptor in 64-bit mode, print raw.

          printf("%016llx", e.raw_value());

          if (e.desc_size() == 16)
            printf(" %016llx", (&e)[1].raw_value());
          else
            printf("%17s", "");
         }

      printf(" p=%u dpl=%u 64-bit system    %02X (\033[33;1m%s\033[m)\n",
             e.present(), e.dpl(), e.type_system(),
             desc_type_system_str64(e));
    }
  else
    {
      // 32-bit descriptor (one entry occupies 8 bytes)

      if (IS_ENABLED(CONFIG_BIT64)
          && (!e.system()
              || (e.type_system() != Gdt_entry::Tss_available
                  && e.type_system() !=  Gdt_entry::Tss_busy)))
        printf("................-................"); // base/limit ignored
      else
        printf("%08lx-%08lx", e.base(), e.base() + e.size());

      printf(" p=%u dpl=%u ", e.present(), e.dpl());

      if (e.system())
        printf("32-bit system    %02X (\033[33;1m%s\033[m)\n", e.type_system(),
                desc_type_system_str32(e));
      else
        printf("%s code/data %02X (\033[33;1m%s%s\033[m)\n",
               desc_size_str(e), e.type(), desc_type_str(e),
               e.accessed() ? " acc" : "");
    }
}



/**
 * Print human-readable gate descriptor contents.
 */
inline static void
desc_show(Idt_entry const &e)
{
  switch (e.type_system())
    {
    case Idt_entry::Task_gate:
      printf("----------------  sel=%04x p=%u dpl=%u %02X "
             "(\033[33;1m%s\033[m)\n", e.selector(), e.present(), e.dpl(),
             e.type_system(), desc_type_system_str(e));
      break;
    case Idt_entry::Intr_gate:
    case Idt_entry::Trap_gate:
      printf("%016lx  sel=%04x p=%u dpl=%u %02X (\033[33;1m%s\033[m)\n",
             e.offset(), e.selector(), e.present(), e.dpl(), e.type_system(),
             desc_type_system_str(e));
      break;
    default:
      printf("%26s p=%u %8s (\033[33;1m%s\033[m)\n", "", e.present(), "",
             desc_type_system_str(e));
    }
}


}


