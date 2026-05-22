
#include <arm_control.h>
#include <static_init.h>
#include <initcalls.h>
#include <globalconfig.h>

// ------------------------------------------------------------------------
#if defined(CONFIG_PF_RPI_RPI2) || defined(CONFIG_PF_RPI_RPI3) \
    || (defined(CONFIG_PF_RPI_RPI4) && !defined(CONFIG_BIT64))
Static_object<Arm_control> Arm_control::_arm_control;
#endif


// ------------------------------------------------------------------------
#if defined(CONFIG_MP) && !defined(CONFIG_BIT64) \
    && (defined(CONFIG_PF_PRI_RPI2) || defined(CONFIG_PF_RPI_RPI3) || defined(CONFIG_PF_RPI_RPI4))

PUBLIC
void
Arm_control::do_boot_cpu(Cpu_phys_id phys_cpu, Address paddr)
{
  unsigned cpu_num = cxx::int_value<Cpu_phys_id>(phys_cpu);
  r.r<32>(Mailbox_set_base + 0xc + cpu_num * 0x10) = paddr;
  Mem::dsb();
  asm volatile("sev");
}
#endif

