#include <mem_space-pcid.h>
#include <mem_space.h>

DEFINE_PER_CPU
Per_cpu<typename Mem_space_ia32_pcid_base::Asid_alloc>
  Mem_space_ia32_pcid_base::_asid_alloc;

