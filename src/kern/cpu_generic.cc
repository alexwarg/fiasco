#include "cpu_generic.h"

Cpu_generic_base::Online_cpu_mask Cpu_generic_base::_online_mask(Online_cpu_mask::Init::Bss);
Cpu_generic_base::Online_cpu_mask Cpu_generic_base::_present_mask(Online_cpu_mask::Init::Bss);

