#pragma once

// Macro to force 32-bit instructions, even on Thumb builds.
#ifdef __thumb__
#define INST32(inst)  inst ".w"
#else
#define INST32(inst)  inst
#endif

