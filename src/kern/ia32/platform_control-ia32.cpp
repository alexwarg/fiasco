IMPLEMENTATION [ia32,amd64]:

PUBLIC static inline void
Platform_control::prepare_cpu_suspend(Cpu_number)
{
  asm volatile ("wbinvd");
}

PUBLIC static inline void FIASCO_NORETURN
Platform_control::cpu_suspend(Cpu_number)
{
  for (;;)
    asm volatile ("cli; wbinvd; hlt");
}

// -----------------------------------------------------------------------
IMPLEMENTATION [mp && (ia32 || amd64)]:

#include "apic.h"

IMPLEMENT int
Platform_control::arch_cpu_hotplug(Cpu_phys_id phys_id)
{
  extern char _tramp_mp_entry[];

  if (!Apic::is_present())
    return -L4_err::ENodev;

  Mword apic_id = cxx::int_value<Cpu_phys_id>(phys_id) << 24;

  // test if CPU is already booted
  Cpu_number id = Apic::find_cpu(apic_id);
  if (EXPECT_FALSE(id != Cpu_number::nil() && Cpu::online(id)))
    return -L4_err::EInval;

  Apic::mp_startup(&Cpu::cpus.current(), apic_id,
                   (Address)&_tramp_mp_entry[0]);
  return 0;
}

