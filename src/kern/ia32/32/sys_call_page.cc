
#include <initcalls.h>
#include <types.h>
#include <static_init.h>
#include <feature.h>
#include <kip.h>
#include <cpu.h>
#include <kernel_task.h>
#include <vmem_alloc.h>
#include <panic.h>

#include <globalconfig.h>

#include <cstdio>
#include <cstring>

KIP_KERNEL_FEATURE("kip_syscalls");

#define SYSCALL_SYMS(sysc) \
extern char sys_call_##sysc, sys_call_##sysc##_end


SYSCALL_SYMS(invoke);
SYSCALL_SYMS(se_invoke);

namespace {

enum
{
  Offs_invoke            = 0x000,
  Offs_se_invoke         = 0x000,
  Offs_kip_invoke        = 0x800,
  Offs_kip_se_invoke     = 0x800,
};


#define INV_SYSCALL(sysc) \
  *reinterpret_cast<Unsigned16*>(Mem_layout::Syscalls + Offs_##sysc) = 0x0b0f

#define COPY_SYSCALL(sysc) do { \
memcpy( (char*)Mem_layout::Syscalls + Offs_##sysc, &sys_call_##sysc, \
        &sys_call_##sysc##_end- &sys_call_##sysc ); \
memcpy( (char*)Kip::k() + Offs_kip_##sysc, &sys_call_##sysc, \
        &sys_call_##sysc##_end- &sys_call_##sysc ); } while (0)



static void
setup_sys_call_page()
{
  if (!Vmem_alloc::page_alloc((void*)Mem_layout::Syscalls,
        Vmem_alloc::ZERO_FILL, Vmem_alloc::User))
    panic("Can't allocate system-call page.");

  printf ("Absolute KIP Syscalls using: %s\n",
          Cpu::have_sysenter() ? "Sysenter" : "int 0x30");

  Kip::k()->kip_sys_calls       = 2;

  if (Cpu::have_sysenter())
    COPY_SYSCALL(se_invoke);
  else
    COPY_SYSCALL(invoke);

  Kernel_task::kernel_task()->set_attributes(
      Virt_addr(Mem_layout::Syscalls),
      Page::Attr(Page::Rights::UR(), Page::Type::Normal(),
                 Page::Kern::Global()));
}

STATIC_INITIALIZER(setup_sys_call_page);

}
