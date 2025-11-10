#include "cpu.h"

#include <cstdio>
#include <cstring>
#include <panic.h>

#include "io.h"
#include "paging.h"
#include "kmem_space.h"
#include "kmem_alloc.h"
#include "mem_unit.h"
#include "processor.h"
#include "ram_quota.h"

DEFINE_PER_CPU_P(0) Per_cpu<Cpu> Cpu::cpus(Per_cpu_data::Cpu_num);
Cpu *Cpu::_boot_cpu;

void
Cpu::init(bool /*resume*/, bool is_boot_cpu)
{
  if (is_boot_cpu)
    {
      _boot_cpu = this;
      set_present(1);
      set_online(1);
    }

  _phys_id = Proc::cpu_id();
  init_tz();
  id_init();
  init_errata_workarounds();
  init_supervisor_mode(is_boot_cpu);
  init_hyp_mode(is_boot_cpu);
}


