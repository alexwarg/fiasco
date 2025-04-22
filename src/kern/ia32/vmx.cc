#include <vmx.h>

#include <cpu.h>
#include <kmem.h>
#include <kmem_alloc.h>
#include <l4_types.h>
#include <cstring>
#include <idt.h>
#include <panic.h>
#include <warn.h>
#include <vm_vmx_asm.h>
#include <entry-ia32.h>

class Vmx_init_host_state
{
  static Per_cpu<Vmx_init_host_state> cpus;

public:
  Vmx_init_host_state(Cpu_number cpu)
  {
    Vmx &v = Vmx::cpus.cpu(cpu);
    Cpu &c = Cpu::cpus.cpu(cpu);

    if (cpu == Cpu::invalid() || !c.vmx() || !v.vmx_enabled())
      return;

    v.vmwrite(Vmx::F_host_es_selector, GDT_DATA_KERNEL);
    v.vmwrite(Vmx::F_host_cs_selector, GDT_CODE_KERNEL);
    v.vmwrite(Vmx::F_host_ss_selector, GDT_DATA_KERNEL);
    v.vmwrite(Vmx::F_host_ds_selector, GDT_DATA_KERNEL);

    /* set FS and GS to unusable in the host state */
    v.vmwrite(Vmx::F_host_fs_selector, 0);
    v.vmwrite(Vmx::F_host_gs_selector, 0);

    Unsigned16 tr = c.get_tr();
    v.vmwrite(Vmx::F_host_tr_selector, tr);

    v.vmwrite(Vmx::F_host_tr_base, ((*c.get_gdt())[tr / 8]).base());
    v.vmwrite(Vmx::F_host_rip, vm_vmx_exit_vec);
    v.vmwrite<Mword>(Vmx::F_host_sysenter_cs, Gdt::gdt_code_kernel);
    v.vmwrite(Vmx::F_host_sysenter_esp, &c.kernel_sp());
    v.vmwrite(Vmx::F_host_sysenter_eip, entry_sys_fast_ipc_c);

    if (c.features() & FEAT_PAT
        && v.info.exit_ctls.allowed(Vmx_info::Ex_load_ia32_pat))
      {
        v.vmwrite(Vmx::F_host_ia32_pat, Cpu::rdmsr(MSR_PAT));
        v.info.exit_ctls.enforce(Vmx_info::Ex_load_ia32_pat, true);
      }
    else
      {
        // We have no proper PAT support, so disallow PAT load store for
        // guest too
        v.info.exit_ctls.enforce(Vmx_info::Ex_save_ia32_pat, false);
        v.info.entry_ctls.enforce(Vmx_info::En_load_ia32_pat, false);
      }

    if (v.info.exit_ctls.allowed(Vmx_info::Ex_load_ia32_efer))
      {
        v.vmwrite(Vmx::F_host_ia32_efer, Cpu::rdmsr(MSR_EFER));
        v.info.exit_ctls.enforce(Vmx_info::Ex_load_ia32_efer, true);
      }
    else
      {
        // We have no EFER load for host, so disallow EFER load store for
        // guest too
        v.info.exit_ctls.enforce(Vmx_info::Ex_save_ia32_efer, false);
        v.info.entry_ctls.enforce(Vmx_info::En_load_ia32_efer, false);
      }

    if (v.info.exit_ctls.allowed(Vmx_info::Ex_load_perf_global_ctl)
        && c.arch_perf_mon_version() > 0)
      v.vmwrite(Vmx::F_host_ia32_perf_global_ctrl, Cpu::rdmsr(0x38f));
    else
      // do not allow Load IA32_PERF_GLOBAL_CTRL on entry
      v.info.entry_ctls.enforce(Vmx_info::En_load_perf_global_ctl, false);

    v.vmwrite(Vmx::F_host_cr0, Cpu::get_cr0());
    v.vmwrite(Vmx::F_host_cr4, Cpu::get_cr4());

    Pseudo_descriptor pseudo;
    c.get_gdt()->get(&pseudo);

    v.vmwrite(Vmx::F_host_gdtr_base, pseudo.base());

    Idt::get(&pseudo);
    v.vmwrite(Vmx::F_host_idtr_base, pseudo.base());

    // init static guest area stuff
    v.vmwrite(0x2800, ~0ULL); // link pointer
    v.vmwrite(Vmx::F_cr3_target_cnt, 0);

    // MSR load / store disabled
    v.vmwrite(Vmx::F_exit_msr_load_cnt, 0);
    v.vmwrite(Vmx::F_exit_msr_store_cnt, 0);
    v.vmwrite(Vmx::F_entry_msr_load_cnt, 0);
  }
};

DEFINE_PER_CPU Per_cpu<Vmx> Vmx::cpus(Per_cpu_data::Cpu_num);
DEFINE_PER_CPU_LATE Per_cpu<Vmx_init_host_state> Vmx_init_host_state::cpus(Per_cpu_data::Cpu_num);

void
Vmx_info::init()
{
  bool ept = false;
  basic = Cpu::rdmsr(0x480);
  pinbased_ctls = Cpu::rdmsr(0x481);
  pinbased_ctls_default1 = pinbased_ctls.must_be_one();
  procbased_ctls = Cpu::rdmsr(0x482);
  procbased_ctls_default1 = procbased_ctls.must_be_one();
  exit_ctls = Cpu::rdmsr(0x483);
  exit_ctls_default1 = exit_ctls.must_be_one();
  entry_ctls = Cpu::rdmsr(0x484);
  entry_ctls_default1 = entry_ctls.must_be_one();
  misc = Cpu::rdmsr(0x485);

  cr0_defs = Bit_defs<Mword>(Cpu::rdmsr(0x486), Cpu::rdmsr(0x487));
  cr4_defs = Bit_defs<Mword>(Cpu::rdmsr(0x488), Cpu::rdmsr(0x489));
  exception_bitmap = Bit_defs_32<Vmx_info::Exceptions>(0xffffffff00000000ULL);

  max_index = Cpu::rdmsr(0x48a);
  if (procbased_ctls.allowed(Vmx_info::PRB1_enable_proc_based_ctls_2))
    procbased_ctls2 = Cpu::rdmsr(0x48b);

  assert ((Vmx::F_sw_guest_cr2 & 0x3ff) > max_index);
  max_index = Vmx::F_sw_guest_cr2 & 0x3ff;
  Vmx_user_info::Fo_table::check_offsets(max_index);

  if (basic & (1ULL << 55))
    {
      // do not use the true pin-based ctls because user-level then needs to
      // be aware of the fact that it has to set bits 1, 2, and 4 to default 1
      if (0) pinbased_ctls = Cpu::rdmsr(0x48d);

      procbased_ctls = Cpu::rdmsr(0x48e);
      exit_ctls = Cpu::rdmsr(0x48f);
      entry_ctls = Cpu::rdmsr(0x490);
    }

  if (0)
    dump("as read from hardware");

  pinbased_ctls.enforce(Vmx_info::PIB_ext_int_exit);
  pinbased_ctls.enforce(Vmx_info::PIB_nmi_exit);


  // currently we IO-passthrough is missing, disable I/O bitmaps and enforce
  // unconditional io exiting
  procbased_ctls.enforce(Vmx_info::PRB1_use_io_bitmaps, false);
  procbased_ctls.enforce(Vmx_info::PRB1_unconditional_io_exit);

  // Always exit if the guest accesses a debug register.
  procbased_ctls.enforce(Vmx_info::PRB1_mov_dr_exit, true);

  procbased_ctls.enforce(Vmx_info::PRB1_use_msr_bitmaps, false);

  // exit on performance counter use
  procbased_ctls.enforce(Vmx_info::PRB1_rdpmc_exiting, true);

  // virtual APIC not yet supported
  procbased_ctls.enforce(Vmx_info::PRB1_tpr_shadow, false);

  if (procbased_ctls.allowed(Vmx_info::PRB1_enable_proc_based_ctls_2))
    {
      procbased_ctls.enforce(Vmx_info::PRB1_enable_proc_based_ctls_2, true);

      if (procbased_ctls2.allowed(Vmx_info::PRB2_enable_ept))
	ept_vpid_cap = Cpu::rdmsr(0x48c);


      // we disable VPID so far, need to handle virtualize it in Fiasco,
      // as done for AMDs ASIDs
      procbased_ctls2.enforce(Vmx_info::PRB2_enable_vpid, false);

      // we do not (yet) support Page Modification Logging (PML)
      procbased_ctls2.enforce(Vmx_info::PRB2_enable_pml, false);

      // EPT only in conjunction with unrestricted guest !!!
      if (procbased_ctls2.allowed(Vmx_info::PRB2_enable_ept))
        {
          ept = true;
          procbased_ctls2.enforce(Vmx_info::PRB2_enable_ept, true);

          if (procbased_ctls2.allowed(Vmx_info::PRB2_unrestricted))
            {
              // unrestricted guest allows PE and PG to be 0
              cr0_defs.relax(0);  // PE
              cr0_defs.relax(31); // PG
              procbased_ctls2.enforce(Vmx_info::PRB2_unrestricted);
            }
          else
            {
              assert (not cr0_defs.allowed(0, false));
              assert (not cr0_defs.allowed(31, false));
            }

          // We currently do not implement the xss bitmap, and do not support
          // the MSR_IA32_XSS which is shared between guest and host. Therefore
          // we disable xsaves/xrstores for the guest.
          procbased_ctls2.enforce(Vmx_info::PRB2_enable_xsaves, false);
        }
      else
        assert (not procbased_ctls2.allowed(Vmx_info::PRB2_unrestricted));
    }
  else
    procbased_ctls2 = 0;

  // never automatically ack interrupts on exit
  exit_ctls.enforce(Vmx_info::Ex_ack_irq_on_exit, false);

  // host-state is 64bit or not
  exit_ctls.enforce(Vmx_info::Ex_host_addr_size, sizeof(long) > sizeof(int));

  if (!ept) // needs to be per VM
    {
      // always enable paging
      cr0_defs.enforce(31);
      // always PE
      cr0_defs.enforce(0);
      cr4_defs.enforce(4); // PSE

      // enforce PAE on 64bit, and disallow it on 32bit
      cr4_defs.enforce(5, sizeof(long) > sizeof(int));
    }

  // allow cr4.vmxe
  cr4_defs.relax(13);

  exception_bitmap.enforce(Vmx_info::Exception_db, true);
  exception_bitmap.enforce(Vmx_info::Exception_ac, true);

  if (0)
    dump("as modified");
}

void
Vmx_info::dump(const char *tag) const
{
  printf("VMX MSRs %s:\n", tag);
  printf("basic                = %16llx\n", basic);
  pinbased_ctls.print("pinbased_ctls");
  procbased_ctls.print("procbased_ctls");
  exit_ctls.print("exit_ctls");
  entry_ctls.print("entry_ctls");
  printf("misc                 = %16llx\n", misc);
  cr0_defs.print("cr0_fixed");
  cr4_defs.print("cr4_fixed");
  procbased_ctls2.print("procbased_ctls2");
  printf("ept_vpid_cap         = %16llx\n", ept_vpid_cap);
  exception_bitmap.print("exception_bitmap");
}

bool
Vmx::handle_bios_lock()
{
  enum
  {
    Feature_control_lock            = 1 << 0,
    Feature_control_vmx_outside_SMX = 1 << 2,
  };

  Unsigned64 feature = Cpu::rdmsr(MSR_IA32_FEATURE_CONTROL);

  if (feature & Feature_control_lock)
    {
      if (!(feature & Feature_control_vmx_outside_SMX))
        return false;
    }
  else
    Cpu::wrmsr(feature | Feature_control_vmx_outside_SMX | Feature_control_lock,
               MSR_IA32_FEATURE_CONTROL);
  return true;
}

void
Vmx::pm_on_resume(Cpu_number)
{
  check (handle_bios_lock());

  // enable vmx operation
  asm volatile("vmxon %0" : : "m"(_vmxon_base_pa) : "cc");

  Mword eflags;
  // make kernel vmcs current
  asm volatile("vmptrld %1 \n\t"
	       "pushf      \n\t"
	       "pop %0     \n\t"
               : "=r"(eflags) : "m"(_kernel_vmcs_pa) : "cc");

  // FIXME: MUST NOT PANIC ON CPU HOTPLUG
  if (eflags & 0x41)
    panic("VMX: vmptrld: VMFailInvalid, vmcs pointer not valid\n");

}

void
Vmx::pm_on_suspend(Cpu_number)
{
  Mword eflags;
  asm volatile("vmclear %1 \n\t"
	       "pushf      \n\t"
	       "pop %0     \n\t"
               "vmxoff     \n\t"
               : "=r"(eflags) : "m"(_kernel_vmcs_pa) : "cc");
  if (eflags & 0x41)
    WARN("VMX: vmclear: vmcs pointer not valid\n");
}

void
Vmx::pm_on_shutdown(Cpu_number cpu)
{
  Vmx::pm_on_suspend(cpu);
}

Vmx::Vmx(Cpu_number cpu)
  : _vmx_enabled(false), _has_vpid(false)
{
  Cpu &c = Cpu::cpus.cpu(cpu);
  if (cpu == Cpu::invalid() || !c.vmx())
    {
      if (cpu == Cpu_number::boot_cpu())
        WARNX(Info, "VMX: Not supported\n");
      return;
    }

  // check whether vmx is enabled by BIOS
  if (!handle_bios_lock())
    {
      if (cpu == Cpu_number::boot_cpu())
        WARNX(Info, "VMX: CPU has VMX support but it is disabled\n");
      return;
    }

  if (cpu == Cpu_number::boot_cpu())
    printf("VMX: enabled\n");

  info.init();

  // check for EPT support
  if (cpu == Cpu_number::boot_cpu())
    {
      if (info.procbased_ctls2.allowed(Vmx_info::PRB2_enable_ept))
        printf("VMX: EPT supported\n");
      else
        printf("VMX: EPT not available\n");
    }

  // check for vpid support
  if (info.procbased_ctls2.allowed(Vmx_info::PRB2_enable_vpid))
    _has_vpid = true;

  c.set_cr4(c.get_cr4() | (1 << 13)); // set CR4.VMXE to 1

  // if NE bit is not set vmxon will fail
  c.set_cr0(c.get_cr0() | (1 << 5));

  enum
  {
    Vmcs_size = 0x1000, // actual size may be different
  };

  Unsigned32 vmcs_size = ((info.basic & (0x1fffULL << 32)) >> 32);

  if (vmcs_size > Vmcs_size)
    {
      WARN("VMX: VMCS size of %u bytes not supported\n", vmcs_size);
      return;
    }

  // allocate a 4kb region for kernel vmcs
  // FIXME: MUST NOT PANIC ON CPU HOTPLUG
  check(_kernel_vmcs = Kmem_alloc::allocator()->alloc(Order(12)));
  _kernel_vmcs_pa = Kmem::virt_to_phys(_kernel_vmcs);
  // clean vmcs
  memset(_kernel_vmcs, 0, vmcs_size);
  // init vmcs with revision identifier
  *(int *)_kernel_vmcs = (info.basic & 0xFFFFFFFF);

  // allocate a 4kb aligned region for VMXON
  // FIXME: MUST NOT PANIC ON CPU HOTPLUG
  check(_vmxon = Kmem_alloc::allocator()->alloc(Order(12)));

  _vmxon_base_pa = Kmem::virt_to_phys(_vmxon);

  // init vmxon region with vmcs revision identifier
  // which is stored in the lower 32 bits of MSR 0x480
  *(unsigned *)_vmxon = (info.basic & 0xFFFFFFFF);

  // enable vmx operation
  asm volatile("vmxon %0" : : "m"(_vmxon_base_pa) : "cc");
  _vmx_enabled = true;

  if (cpu == Cpu_number::boot_cpu())
    printf("VMX: initialized\n");

  Mword eflags;
  asm volatile("vmclear %1 \n\t"
	       "pushf      \n\t"
	       "pop %0     \n\t"
               : "=r"(eflags) : "m"(_kernel_vmcs_pa) : "cc");
  // FIXME: MUST NOT PANIC ON CPU HOTPLUG
  if (eflags & 0x41)
    panic("VMX: vmclear: VMFailInvalid, vmcs pointer not valid\n");

  // make kernel vmcs current
  asm volatile("vmptrld %1 \n\t"
	       "pushf      \n\t"
	       "pop %0     \n\t"
               : "=r"(eflags) : "m"(_kernel_vmcs_pa) : "cc");

  // FIXME: MUST NOT PANIC ON CPU HOTPLUG
  if (eflags & 0x41)
    panic("VMX: vmptrld: VMFailInvalid, vmcs pointer not valid\n");

  Pm_object::register_pm(cpu);
}

/// Some compile-time VMCS field calculations
namespace Vmcs_field {

/**
 * Calculate the shift needed to calculate the memory offset from a
 * field offset for a field of size FIELD_SIZE (bits 13..14 of a VMCS field index).
 */
template<unsigned FIELD_SIZE> struct Shift;
template<> struct Shift<0> { enum { value = 0 }; }; ///< 16bit -> 1 byte per index
template<> struct Shift<1> { enum { value = 2 }; }; ///< 64bit -> 4 byte per index
template<> struct Shift<2> { enum { value = 1 }; }; ///< 32bit -> 2 byte per index
template<> struct Shift<3> { enum { value = 2 }; }; ///< nat   -> 4 byte per index

/**
 * Calculate the maximum field index of all given fields
 * Uses bits 0..9 of the given index values.
 */
template<unsigned ...N> struct Max;
template<unsigned A1> struct Max<A1> { enum { value = A1 & 0x3ff }; };
template<unsigned A1, unsigned A2, unsigned ...N>
struct Max<A1, A2, N...>
{
  enum
  {
    value = ((A1 & 0x3ff) > (A2 & 0x3ff))
            ? (unsigned)Max<A1 & 0x3ff, N...>::value
            : (unsigned)Max<A2 & 0x3ff, N...>::value
  };
};

enum
{
  /**
   * Max of all of our defined field indices.
   *
   * We calculate this without host fields, as host fields are never
   * exposed to in our API.
   */
  Max_field_index = Max<Vmx::F_max_16bit_ctl,
                        Vmx::F_max_16bit_guest,
                        Vmx::F_max_64bit_ctl,
                        Vmx::F_max_64bit_ro,
                        Vmx::F_max_64bit_guest,
                        Vmx::F_max_32bit_ctl,
                        Vmx::F_max_32bit_ro,
                        Vmx::F_max_32bit_guest,
                        Vmx::F_max_nat_ctl,
                        Vmx::F_max_nat_ro,
                        Vmx::F_max_nat_guest>::value
};

/**
 * Calculate the size (in multiples of 64 bytes) of a field block given
 * the maximum field index in a group.
 */
template<unsigned FIELD_MAX>
struct Block_size
{
  enum
  { value = ((((FIELD_MAX & 0x3ff) + 1) << Shift<(FIELD_MAX >> 13)>::value) + 63) / 64 };
};

enum
{
  Offset_0000 = 64 / 64,
  Offset_2000 = Offset_0000 + Block_size<0x0000 | Max_field_index>::value,
  Offset_4000 = Offset_2000 + Block_size<0x2000 | Max_field_index>::value,
  Offset_6000 = Offset_4000 + Block_size<0x4000 | Max_field_index>::value,

  Offset_0400 = Offset_6000 + Block_size<0x6000 | Max_field_index>::value,
  Offset_2400 = Offset_0400 + Block_size<0x0400 | Max_field_index>::value,
  Offset_4400 = Offset_2400 + Block_size<0x2400 | Max_field_index>::value,
  Offset_6400 = Offset_4400 + Block_size<0x4400 | Max_field_index>::value,

  Offset_0800 = Offset_6400 + Block_size<0x6400 | Max_field_index>::value,
  Offset_2800 = Offset_0800 + Block_size<0x0800 | Max_field_index>::value,
  Offset_4800 = Offset_2800 + Block_size<0x2800 | Max_field_index>::value,
  Offset_6800 = Offset_4800 + Block_size<0x4800 | Max_field_index>::value,

  Total_size  = Offset_6800 + Block_size<0x6800 | Max_field_index>::value
};

static_assert(Total_size * 64 + 1024 < 4096, "VMCS fields exceed extended vCPU state");
}

/*
 * VMCS field offset table:
 *  0h -  2h: 3 offsets for 16bit fields:
 *            0: Control fields, 1: read-only fields, 2: guest state
 *            all offsets in 64byte granules relative to the start of the VMCS
 *        3h: Reserved
 *  4h -  7h: Index shift values for 16bit, 64bit, 32bit, and natural width fields
 *  8h -  Ah: 3 offsets for 64bit fields
 *  Bh -  Fh: Reserved
 * 10h - 12h: 3 offsets for 32bit fields
 * 13h - 17h: Reserved
 * 18h - 1Ah: 3 offsets for natural width fields
 *       1Bh: Reserved
 *       1Ch: Offset of first VMCS field
 *       1Dh: Full size of VMCS fields
 * 1Eh - 1Fh: Reserved
 *
 */
unsigned char const Vmx_user_info::Fo_table::master_offsets[32] =
{
   Vmcs_field::Offset_0000, Vmcs_field::Offset_0400, Vmcs_field::Offset_0800, 0,   0, 2, 1, 2,
   Vmcs_field::Offset_2000, Vmcs_field::Offset_2400, Vmcs_field::Offset_2800, 0,   0, 0, 0, 0,
   Vmcs_field::Offset_4000, Vmcs_field::Offset_4400, Vmcs_field::Offset_4800, 0,   0, 0, 0, 0,
   Vmcs_field::Offset_6000, Vmcs_field::Offset_6400, Vmcs_field::Offset_6800, 0,
   Vmcs_field::Offset_0000, Vmcs_field::Total_size, 0, 0,
};


