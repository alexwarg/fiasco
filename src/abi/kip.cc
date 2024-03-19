
#include "kip.h"

#include "version.h"
#include "static_assert.h"


Kip *Kip::global_kip;

char const *Kip::version_string() const
{
  static_assert((sizeof(Kip) & 0xf) == 0, "Invalid KIP structure size");

  return reinterpret_cast <char const *> (this) + (offset_version_strings << 4);
}

#ifdef TARGET_NAME
#define TARGET_NAME_PHRASE " for " TARGET_NAME
#else
#define TARGET_NAME_PHRASE
#endif

asm(".section .initkip.version, \"a\", %progbits        \n"
    ".string \"" CONFIG_KERNEL_VERSION_STRING "\"       \n"
    ".previous                                          \n");

asm(".section .initkip.features.end, \"a\", %progbits   \n"
    ".string \"\"                                       \n"
    ".previous                                          \n");



