
#include <pfc-arm.h>
#include "platform_arm_realview.h"
#include <types.h>
#include <ipi.h>
#include <globalconfig.h>
#include <infinite_loop.h>

struct Pfc_realview : Pfc_arm
{
#ifdef CONFIG_MP
  void do_boot_ap_cpus(Address phys_tramp_mp_addr) override
  {
    // set physical start address for AP CPUs
    Platform::sys->write<Mword>(0xffffffff, Platform::Sys::Flags_clr);
    Platform::sys->write<Mword>(phys_tramp_mp_addr, Platform::Sys::Flags);

    // wake up AP CPUs, always from CPU 0
    Ipi::bcast(Ipi::Global_request, Cpu_number::boot_cpu());
  }
#endif
};

struct Pfc_rv_eb : Pfc_realview
{
  void system_reboot() override
  {
    // unlock for reset
    Platform::sys->write<Mword>(0xa05f, Platform::Sys::Lock);
    // the 0x100 is for Qemu
    Platform::sys->write<Mword>(0x108, Platform::Sys::Reset);
  }
};

struct Pfc_rv_pb11mp : Pfc_realview
{
  void system_reboot() override
  {
    // unlock for reset
    Platform::sys->write<Mword>(0xa05f, Platform::Sys::Lock);
    // PORESET (0x8 would also be ok)
    Platform::sys->write<Mword>(0x4, Platform::Sys::Reset);
  }
};

struct Pfc_rv_vexpress : Pfc_realview
{
  void system_reboot() override
  {
    // unlock for reset
    Platform::sys->write<Mword>(0xa05f, Platform::Sys::Lock);
    // POWER reset, 0x100 for Qemu
    Platform::sys->write<Mword>(0x104, Platform::Sys::Reset);
  }
};


#ifdef CONFIG_PF_REALVIEW_EB
using Pfc_rv = Pfc_rv_eb;
#endif
#ifdef CONFIG_PF_REALVIEW_PB11MP
using Pfc_rv = Pfc_rv_pb11mp;
#endif
#ifdef CONFIG_PF_REALVIEW_PBX
using Pfc_rv = Pfc_rv_vexpress;
#endif
#ifdef CONFIG_PF_REALVIEW_VEXPRESS
using Pfc_rv = Pfc_rv_vexpress;
#endif

static Pfc_singleton<Pfc_rv> __pfc;
