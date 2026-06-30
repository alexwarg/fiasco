#pragma once

struct Alternative_insn_entry
{
  bool (*probe)(); ///< function called to determine which version is used
  Signed32 disabled; ///< offset of the "disabled" instruction relative to `this`
  Signed32 enabled; ///< offset of the "enabled" instruction relative to `this`
  Unsigned8 len; ///< Number of bytes in the code

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


#if defined(__aarch64__)
# define ASM_ALTERNATIVE_ENTRY_PTR ".dword %c[alt_probe]"
# define ASM_ALTERNATIVE_ENTRY_DEP(l) ".subsection 10\n\t.reloc 0, R_AARCH64_NONE, " #l "\n\t.previous\n\t"
#elif defined(__arm__)
# define ASM_ALTERNATIVE_ENTRY_PTR ".word %c[alt_probe]"
# define ASM_ALTERNATIVE_ENTRY_DEP(l) ".subsection 10\n\t.reloc 0, R_ARM_NONE, " #l "\n\t.previous\n\t"
#endif

#define ARCH_ALTERNATIVE_ASM_GOTO(probe, no) \
  ALTERNATIVE_INSN("b %l[no]", "nop") : : [alt_probe] "Si"(probe) : : no

