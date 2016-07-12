#pragma once

#include <types.h>

/**
 * Single feature dependent instruction alternative.
 *
 * Alternatives are patched at boot time to replace feature dependent
 * instructions.
 *
 * The current implementation is limited to patching a single ASM (32bit)
 * instruction.
 *
 * An array of Alternative_insn structures is provided between
 * _alt_insns_begin and _alt_insns_end.
 */
struct Alternative_insn
{
  Signed32 orig; ///< offset of the original instruction relative to `this`
  Signed32 alt;  ///< offset of the alternative instruction relative to `this`
  Unsigned16 feature;  ///< feature value compared agains the masked options
  Unsigned16 mask;     ///< feature bit mask select significant option bits
  Unsigned8 total_len; ///< Total number of bytes in the code
  Unsigned8 r_len;     ///< Length of this replacement in bytes

  Unsigned32 *orig_code() const
  {
    return reinterpret_cast<Unsigned32*>(reinterpret_cast<Address>(this) + orig);
  }

  Unsigned32 const *alt_insn() const
  {
    return reinterpret_cast<Unsigned32*>(reinterpret_cast<Address>(this) + alt);
  }

  static void handle_alternatives(unsigned features);

private:
  void replace() const;
};

#define ASM_ALTERNATIVE_ENTRY(feature, idx) \
        "888:                      \n\t"    \
        ".long 819b - 888b         \n\t"    \
        ".long 889" # idx "f - 888b\n\t"    \
        ".half " # feature "       \n\t"    \
        ".half " # feature "       \n\t"    \
        ".byte 839b - 819b         \n\t"    \
        ".byte 8991f - 8891f       \n\t"

#define ALTERNATIVE_INSN(orig_insn, new_insn, feature)    \
        "819:\n\t"                                        \
        orig_insn "\n\t"                                  \
        "829:\n\t"                                        \
        ".skip -(((8991f - 8891f) - (829b - 819b)) > 0) * "   \
        "        ((8991f - 8891f) - (829b - 819b)), 0x00\n\t" \
        "839:\n\t"                                        \
        ".pushsection .alt_insns, \"a?\"\n\t"             \
        ASM_ALTERNATIVE_ENTRY(feature, 1)                 \
        ".popsection \n\t"                                \
        ".pushsection .alt_insn_replacement, \"ax\" \n\t" \
        "8891:\n\t"                                       \
        new_insn "\n\t"                                   \
        "8991:\n\t"                                       \
        ".popsection                   \n\t"

