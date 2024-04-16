#include <arm_ipis.h>
#include <globalconfig.h>

#ifdef CONFIG_PF_EXYNOS_EXTGIC

#include <pic.h>
#include <per_cpu_data.h>

class Arm_ipis_ex : public Arm_ipis::Base
{
public:
  Arm_ipis_ex(Cpu_number cpu, bool resume)
  {
    if (!resume)
      {
        check(Pic::gic.cpu(cpu)->alloc(&remote_rq_ipi, Ipi::Request));
        check(Pic::gic.cpu(cpu)->alloc(&glbl_remote_rq_ipi, Ipi::Global_request));
        check(Pic::gic.cpu(cpu)->alloc(&debug_ipi, Ipi::Debug));
        check(Pic::gic.cpu(cpu)->alloc(&timer_ipi, Ipi::Timer));
      }
  }
};

DEFINE_PER_CPU static Per_cpu<Static_object<Arm_ipis_ex> > _arm_ipis;

void
Arm_ipis::init_per_cpu(Cpu_number cpu, bool resume)
{
  _arm_ipis.cpu(cpu).construct(cpu, resume);
}

#else // CONFIG_PF_EXYNOS_EXTGIC

void
Arm_ipis::init_per_cpu(Cpu_number cpu, bool resume)
{}

static Arm_ipis::Ipis _arm_ipis;

#endif // CONFIG_PF_EXYNOS_EXTGIC

