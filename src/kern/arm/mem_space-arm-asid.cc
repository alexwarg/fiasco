#include <mem_space-arm-asid.h>

DEFINE_PER_CPU Per_cpu<Mem_space_arm_asid::Asids> Mem_space_arm_asid::_asids;
Mem_space_arm_asid::Asid_alloc  Mem_space_arm_asid::_asid_alloc(&_asids);

