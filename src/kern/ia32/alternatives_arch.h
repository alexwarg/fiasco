
#pragma once

struct Alternative_insn_entry
{
  bool (*probe)(); ///< function called to determine which version is used
  Signed32 disabled; ///< offset of the "disabled" instruction relative to `this`
  Signed32 enabled; ///< offset of the "enabled" instruction relative to `this`
  Unsigned8 len; ///< Number of bytes in the code
  Unsigned8 rlen; ///< Number of bytes in the replacement code

  void *disabled_insn() const
  {
    return offset_cast<void *>(this, disabled);
  }

  void const *enabled_insn() const
  {
    return offset_cast<void *>(this, enabled);
  }

  void enable() const;
} __attribute__((packed));


#if defined(__x86_64__)
# define ASM_ALTERNATIVE_ENTRY_PTR ".quad %c[alt_probe]"
# define ASM_ALTERNATIVE_ENTRY_DEP(l) ".subsection 10\n\t.reloc 0, R_X86_64_NONE, " #l "\n\t.previous\n\t"
#elif defined(__i386__) || defined(__i686__)
# define ASM_ALTERNATIVE_ENTRY_PTR ".long %c[alt_probe]"
# define ASM_ALTERNATIVE_ENTRY_DEP(l) ".subsection 10\n\t.reloc 0, R_386_NONE, " #l "\n\t.previous\n\t"
#else
# error "Missing architecture specific defines!"
#endif

#define _ALTERNATIVE_alt_start(num) "888" # num

#define _ALTERNATIVE_ENTRY(num, alt_ofs, alt_len)       \
  ".pushsection .alt_insns, \"a?\"\n"                   \
  _ALTERNATIVE_alt_start(num) ":\n\t"                                \
  ASM_ALTERNATIVE_ENTRY_PTR "\n\t"                      \
  ".4byte 819b - " _ALTERNATIVE_alt_start(num) "b\n\t"               \
  ".4byte " #alt_ofs "\n\t"                             \
  ".byte 839b - 819b\n\t"                               \
  ".byte " #alt_len "\n"                                \
  ".popsection\n"



#define ALTERNATIVE_INSN_ENABLED_NOP(disabled_insn)           \
        "819:                                           \n\t" \
        disabled_insn                                  "\n\t" \
        "829:\n839:                                     \n\t" \
        ASM_ALTERNATIVE_ENTRY_DEP(8881f)                      \
        _ALTERNATIVE_ENTRY(1, 0, 0)

#define ALTERNATIVE_INSN(disabled_insn, enabled_insn)         \
        "# ALTERNATIVE: oirg / disabled\n"                    \
        "819:\n\t"                                            \
        disabled_insn "\n"                                    \
        "829:\n\t"                                            \
        "# ALTERNATIVE: padding\n"                            \
        ".skip -(((8991f-8891f) - (829b-819b)) > 0) * "       \
                "((8991f-8891f) - (829b-819b)), 0x90\n"       \
        "839:\n"                                              \
        ASM_ALTERNATIVE_ENTRY_DEP(8881f)                      \
        ".pushsection .alt_insn_replacement, \"ax?\" \n\t"    \
        "8891:\n\t"                                           \
        enabled_insn "\n"                                     \
        "8991:\n"                                             \
        ".popsection\n"                                       \
        _ALTERNATIVE_ENTRY(1, 8891b-8881b, 8991b-8891b)

#define ARCH_ALTERNATIVE_ASM_GOTO(probe, no) \
  ALTERNATIVE_INSN_ENABLED_NOP(".byte 0xe9; .4byte %l[no] - 1f; 1:") : : [alt_probe] "i"(probe) : : no

