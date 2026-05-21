
#include <irqs_s3c2410.h>
#include <platform_generic.h>
#include <static_init.h>

namespace {

struct S3c2410_pf : Platform_base
{
  void init_irqs() override
  {
    Arm_s3c2410::create_irq_mgr(true);
  }
};

[[gnu::init_priority(EARLY_INIT_PRIO)]]
static S3c2410_pf __pf;

}
