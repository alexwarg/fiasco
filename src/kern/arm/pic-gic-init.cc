
#include <pic.h>
#include <gic_iface.h>
#include <panic.h>
#include <initcalls.h>
#include <pic-gic-helper.h>

void Pic::init_ap(Cpu_number cpu, bool resume)
{
  Gic::primary->init_ap(cpu, resume);
}

FIASCO_INIT
void Pic::init()
{
  if (int res = Pic_gic::add_gic())
    panic("Could not setup GIC: error=%d\n", res);
}

