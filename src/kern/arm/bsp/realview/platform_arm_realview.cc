#include "platform_arm_realview.h"

#include "kmem_mmio.h"
#include "static_init.h"
#include "rv_platforms.h"

Static_object<Platform::Sys> Platform::sys;
Static_object<Platform::System_control> Platform::system_control;

static void platform_init()
{
  if (Platform::sys->get_mmio_base())
    return;

  auto p = rv_current_platform();
  Platform::sys.construct(Kmem_mmio::map(p->sys_r, 0x1000));
  Platform::system_control.construct(Kmem_mmio::map(p->sys_c, 0x1000));
}

STATIC_INITIALIZER_P(platform_init, ROOT_FACTORY_INIT_PRIO);
