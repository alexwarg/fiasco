
#include <paging-ptab.h>
#include <globalconfig.h>

bool Pt_entry::_have_superpages;
unsigned  Pt_entry::_super_level;

#ifndef CONFIG_KERNEL_ISOLATION
Unsigned32 Pt_entry::_cpu_global = Pt_entry::L4_global;
#endif
