#pragma once

#include <pfc.h>
#include <globalconfig.h>

#ifdef CONFIG_MP
#include <apic.h>
#endif

class Pfc_ia32 : public virtual Pfc
{
public:
  // helpers for implementation
  [[noreturn]] static void cpu_suspend(Cpu_number)
  {
    for (;;)
      asm volatile ("cli; wbinvd; hlt");
  }

  static void prepare_cpu_suspend(Cpu_number)
  {
    asm volatile ("wbinvd");
  }

  void boot_ap_cpus() override
  {
#ifdef CONFIG_MP
    // where to start the APs for detection of the APIC-IDs
    extern char _tramp_mp_entry[];

    // feature enabling flags (esp. cache enabled flag and paging enabled flag)
    extern volatile Mword _tramp_mp_startup_cr0;

    // feature enabling flags (esp. needed for big pages)
    extern volatile Mword _tramp_mp_startup_cr4;

    // physical address of the page table directory to use
    extern volatile Address _realmode_startup_pdbr;

    // pseudo descriptor for the gdt to load
    extern Pseudo_descriptor _tramp_mp_startup_gdt_pdesc;

    Address tramp_page;

    _realmode_startup_pdbr = Kmem::get_realmode_startup_pdbr();

    _tramp_mp_startup_cr4 = Cpu::get_cr4();
    _tramp_mp_startup_cr0 = Cpu::get_cr0();
    _tramp_mp_startup_gdt_pdesc
      = Pseudo_descriptor((Address)Cpu::boot_cpu()->get_gdt(), Gdt::gdt_max -1);

    __asm__ __volatile__ ("" : : : "memory");

    // Say what we do
    printf("MP: detecting APs...\n");

    // broadcast an AP startup via the APIC (let run the self-registration code)
    tramp_page = (Address)&(_tramp_mp_entry[0]);

    // Send IPI-Sequency to startup the APs
    Apic::mp_startup(Cpu::boot_cpu(), Apic::APIC_IPI_OTHERS, tramp_page);
#endif
  }

  int hotplug_cpu(Cpu_phys_id phys_id) override
  {
#ifdef CONFIG_MP
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
#else
    (void) phys_id;
    return -L4_err::ENodev;
#endif
  }

};
