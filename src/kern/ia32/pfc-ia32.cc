#include <pfc.h>
#include <pfc-acpi.h>
#include <pfc-ia32.h>

struct Pfc_pc : Pfc_acpi<Pfc_ia32>
{
};

static Pfc_singleton<Pfc_pc> __pfc_pc;
