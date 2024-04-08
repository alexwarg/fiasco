
#include "trap_state.h"


char const *
Trap_state::exc_code_to_str(Mword cause)
{
  char const *exc_codes[] =
  {
    "IRQ",     "TLB Mod",  "TLBL",      "TLBS",
    "AdEL",    "AdES",     "IBE",       "DBE",
    "Sys",     "Bp",       "RI",        "CpU",
    "Ov",      "Tr",       "MSAFPE",    "FPE",
    "Impl1",   "Impl2",    "C2E",       "TLBRI",
    "TLBXI",   "MSADis",   "MDMX",      "WATCH",
    "MCheck",  "Thread",   "DSPDis",    "GE",
    "Res0",    "Res1",     "CacheErr",  "Res2"
  };
  return exc_codes[(cause >> 2) & 0x1f];
}

