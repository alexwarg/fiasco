#pragma once

#include <per_cpu_data.h>
#include <pm.h>

#include <vmx_bits.h>
#include <warn.h>

#include <cassert>
#include <cstdio>
#include <cstring>

class Vmx_info
{
public:

  template<typename T>
  class Bit_defs
  {
  protected:
    T _or;
    T _and;

    void enforce_bits(T m, bool value = true)
    {
      if (value)
        _or |= m;
      else
        _and &= ~m;
    }

    bool allowed_bits(T m, bool value = true) const
    {
      if (value)
        return _and & m;
      else
        return !(_or & m);
    }

  public:
    Bit_defs() {}
    Bit_defs(T _or, T _and) : _or(_or), _and(_and) {}

    T must_be_one() const { return _or; }
    T may_be_one() const { return _and; }

    T apply(T v) const { return (v | _or) & _and; }

    void print(char const *name) const
    {
      if (sizeof(T) <= 4)
        printf("%20s = %8x %8x\n", name, (unsigned)_and, (unsigned)_or);
      else if (sizeof(T) <= 8)
        printf("%20s = %16llx %16llx\n", name, (unsigned long long)_and,
               (unsigned long long)_or);
    }
  };

  template<typename WORD_TYPE, typename BITS_TYPE>
  struct Bit_defs_t : Bit_defs<WORD_TYPE>
  {
    Bit_defs_t() = default;
    Bit_defs_t(WORD_TYPE _and, WORD_TYPE _or)
    : Bit_defs<WORD_TYPE>(_and, _or) {}

    Bit_defs_t(Bit_defs<WORD_TYPE> const &o) : Bit_defs<WORD_TYPE>(o) {}

    void relax(BITS_TYPE bit)
    {
      this->_or &= ~(WORD_TYPE(1) << WORD_TYPE(bit));
      this->_and |= WORD_TYPE(1) << WORD_TYPE(bit);
    }

    void enforce(BITS_TYPE bit, bool value = true)
    { this->enforce_bits((WORD_TYPE)1 << (WORD_TYPE)bit, value); }

    bool allowed(BITS_TYPE bit, bool value = true) const
    { return this->allowed_bits((WORD_TYPE)1 << (WORD_TYPE)bit, value); }
  };

  template<typename BITS_TYPE>
  class Bit_defs_32 : public Bit_defs_t<Unsigned32, BITS_TYPE>
  {
  public:
    Bit_defs_32() {}
    Bit_defs_32(Unsigned64 v)
    : Bit_defs_t<Unsigned32, BITS_TYPE>(v, v >> 32)
    {}
  };

  typedef Bit_defs<Unsigned64> Bit_defs_64;

  template<typename T>
  class Flags
  {
  public:
    Flags() {}
    explicit Flags(T v) : _f(v) {}

    T test(unsigned char bit) const { return _f & ((T)1 << (T)bit); }
  private:
    T _f;
  };

  enum Pin_based_ctls
  {
    PIB_ext_int_exit = 0,
    PIB_nmi_exit     = 3,
  };

  enum Primary_proc_based_ctls
  {
    PRB1_tpr_shadow               = 21,
    PRB1_unconditional_io_exit    = 24,
    PRB1_use_io_bitmaps           = 25,
    PRB1_use_msr_bitmaps          = 28,
    PRB1_enable_proc_based_ctls_2 = 31,
  };

  enum Secondary_proc_based_ctls
  {
    PRB2_virtualize_apic = 0,
    PRB2_enable_ept      = 1,
    PRB2_enable_vpid     = 5,
    PRB2_unrestricted    = 7,
    PRB2_enable_pml      = 17,
    PRB2_enable_xsaves   = 20,
  };

  enum Entry_ctls
  {
    En_load_debug_ctls       = 2,
    En_ia32e_mode_guest      = 9,
    En_entry_to_smm          = 10,
    En_no_dual_monitor       = 11,
    En_load_perf_global_ctl  = 13,
    En_load_ia32_pat         = 14,
    En_load_ia32_efer        = 15,
  };

  enum Exit_ctls
  {
    Ex_save_debug_ctls        = 2,
    Ex_host_addr_size         = 9,
    Ex_load_perf_global_ctl   = 12,
    Ex_ack_irq_on_exit        = 15,
    Ex_save_ia32_pat          = 18,
    Ex_load_ia32_pat          = 19,
    Ex_save_ia32_efer         = 20,
    Ex_load_ia32_efer         = 21,
    Ex_save_preemption_timer  = 22,
  };

  enum Exceptions
  {
    Exception_db                 = 1,
    Exception_ac                 = 17,
  };

  Unsigned64 basic;

  Bit_defs_32<Pin_based_ctls> pinbased_ctls;
  Bit_defs_32<Primary_proc_based_ctls> procbased_ctls;

  Bit_defs_32<Exit_ctls> exit_ctls;
  Bit_defs_32<Entry_ctls> entry_ctls;
  Unsigned64 misc;

  Bit_defs_t<Mword, unsigned char> cr0_defs;
  Bit_defs_t<Mword, unsigned char> cr4_defs;
  Bit_defs_32<Secondary_proc_based_ctls> procbased_ctls2;
  Bit_defs_32<Exceptions> exception_bitmap;

  Unsigned64 ept_vpid_cap;
  Unsigned64 max_index;
  Unsigned32 pinbased_ctls_default1;
  Unsigned32 procbased_ctls_default1;
  Unsigned32 exit_ctls_default1;
  Unsigned32 entry_ctls_default1;

  void init();
  void dump(const char *tag) const;
};


struct Vmx_user_info
{
  struct Fo_table
  {
    unsigned char offsets[32];
    static unsigned char const master_offsets[32];

    enum
    {
      Foi_size = 4
    };

    template<typename T>
    static T *field(T *b, unsigned vm_field)
    {
      return (void*)((Address)b + master_offsets[vm_field >> 10] * 64
             + ((vm_field & 0x3ff) << master_offsets[Foi_size + (vm_field >> 13)]));
    }

    void init()
    { memcpy(offsets, master_offsets, sizeof(offsets)); }

    static void check_offsets(unsigned max_idx)
    {
      for (unsigned t1 = 0; t1 < 3; ++t1)
        for (unsigned w1 = 0; w1 < 4; ++w1)
          for (unsigned t2 = 0; t2 < 3; ++t2)
            for (unsigned w2 = 0; w2 < 4; ++w2)
              if (t1 != t2 || w1 != w2)
                {
                  unsigned s1 = ((t1 << 10) | (w1 << 13));
                  unsigned s2 = ((t2 << 10) | (w2 << 13));
                  unsigned e1 = s1 | max_idx;
                  unsigned e2 = s2 | max_idx;
                  assert (field((void*)0, s1) > field((void*)0, e2)
                          || field((void*)0, s2) > field((void*)0, e1));
                  (void) s1; (void) s2; (void) e1; (void) e2;
                }
    }
  };

  Unsigned64 basic;
  Vmx_info::Bit_defs_32<Vmx_info::Pin_based_ctls> pinbased;
  Vmx_info::Bit_defs_32<Vmx_info::Primary_proc_based_ctls> procbased;
  Vmx_info::Bit_defs_32<Vmx_info::Exit_ctls> exit;
  Vmx_info::Bit_defs_32<Vmx_info::Entry_ctls> entry;
  Unsigned64 misc;
  Unsigned64 cr0_or;
  Unsigned64 cr0_and;
  Unsigned64 cr4_or;
  Unsigned64 cr4_and;
  Unsigned64 vmcs_field_info;
  Vmx_info::Bit_defs_32<Vmx_info::Secondary_proc_based_ctls> procbased2;
  Unsigned64 ept_vpid_cap;
  Unsigned32 pinbased_dfl1;
  Unsigned32 procbased_dfl1;
  Unsigned32 exit_dfl1;
  Unsigned32 entry_dfl1;
};

class Vmx : public Pm_object
{
public:
  enum : Mword
  { Gpregs_words = Vmx_bits::Gpregs_words };

  enum Vmcs_16bit_ctl_fields
  {
    F_vpid               = 0x0,
    F_posted_irq_vector  = 0x2,
    F_eptp_index         = 0x4,

    // must be the last
    F_max_16bit_ctl
  };

  enum Vmcs_16bit_guest_fields
  {
    F_guest_es               = 0x800,
    F_guest_cs               = 0x802,
    F_guest_ss               = 0x804,
    F_guest_ds               = 0x806,
    F_guest_fs               = 0x808,
    F_guest_gs               = 0x80a,
    F_guest_ldtr             = 0x80c,
    F_guest_tr               = 0x80e,
    F_guest_guest_irq_status = 0x810,

    // must be the last
    F_max_16bit_guest
  };

  enum Vmcs_16bit_host_fields
  {
    F_host_es_selector   = 0x0c00,
    F_host_cs_selector   = 0x0c02,
    F_host_ss_selector   = 0x0c04,
    F_host_ds_selector   = 0x0c06,
    F_host_fs_selector   = 0x0c08,
    F_host_gs_selector   = 0x0c0a,
    F_host_tr_selector   = 0x0c0c,
  };

  enum Vmcs_64bit_ctl_fields
  {
    F_tsc_offset         = 0x2010,
    F_apic_access_addr   = 0x2014,
    F_ept_ptr            = 0x201a,

    // .. skip ...

    F_xss_exiting        = 0x202c,

    // must be the last
    F_max_64bit_ctl
  };

  enum Vmcs_64bit_ro_fields
  {
    F_guest_phys         = 0x2400,

    // must be the last
    F_max_64bit_ro
  };

  enum Vmcs_64bit_guest_fields
  {
    F_guest_pat             = 0x2804,
    F_guest_efer            = 0x2806,
    F_guest_perf_global_ctl = 0x2808,

    // ... skip ...

    F_guest_pdpte3          = 0x2810,

    F_sw_guest_xcr0         = 0x2840,
    F_sw_msr_syscall_mask   = 0x2842,
    F_sw_msr_lstar          = 0x2844,
    F_sw_msr_cstar          = 0x2846,
    F_sw_msr_tsc_aux        = 0x2848,
    F_sw_msr_star           = 0x284a,
    F_sw_msr_kernel_gs_base = 0x284c,

    // must be the last
    F_max_64bit_guest
  };

  enum Vmcs_64bit_host_fields
  {
    F_host_ia32_pat              = 0x2c00,
    F_host_ia32_efer             = 0x2c02,
    F_host_ia32_perf_global_ctrl = 0x2c04,
  };

  enum Vmcs_32bit_ctl_fields
  {
    F_pin_based_ctls       = 0x4000,
    F_proc_based_ctls      = 0x4002,
    F_exception_bitmap     = 0x4004,

    F_cr3_target_cnt       = 0x400a,
    F_exit_ctls            = 0x400c,
    F_exit_msr_store_cnt   = 0x400e,
    F_exit_msr_load_cnt    = 0x4010,
    F_entry_ctls           = 0x4012,
    F_entry_msr_load_cnt   = 0x4014,
    F_entry_int_info       = 0x4016,

    F_entry_exc_error_code = 0x4018,
    F_entry_insn_len       = 0x401a,
    F_proc_based_ctls_2    = 0x401e,
    F_ple_gap              = 0x4020,
    F_ple_window           = 0x4022,

    // must be the last
    F_max_32bit_ctl
  };

  enum Vmcs_32bit_ro_fields
  {
    F_vm_instruction_error = 0x4400,
    F_exit_reason          = 0x4402,
    F_vectoring_info       = 0x4408,
    F_vectoring_error_code = 0x440a,
    F_exit_insn_len        = 0x440c,
    F_exit_insn_info       = 0x440e,

    // must be the last
    F_max_32bit_ro
  };

  enum Vmcs_32bit_guest_fields
  {
    // ... skip ...
    F_sysenter_cs        = 0x482a,
    F_preempt_timer      = 0x482e,

    // must be the last
    F_max_32bit_guest
  };

  enum Vmcs_32bit_host_fields
  {
    F_host_sysenter_cs   = 0x4c00,
  };

  enum Vmcs_nat_ctl_fields
  {
    // ... skip ....
    F_cr3_target_3 = 0x600e,

    // must be the last
    F_max_nat_ctl
  };

  enum Vmcs_nat_ro_fields
  {
    // ... skip ...
    F_guest_linear       = 0x640a,

    // must be the last
    F_max_nat_ro
  };

  enum Vmcs_nat_guest_fields
  {
    F_guest_cr3               = 0x6802,
    // ... skip ...
    F_guest_ia32_sysenter_eip = 0x6826,

    F_sw_guest_cr2            = 0x683e,

    // must be the last
    F_max_nat_guest
  };

  enum Vmcs_nat_host_fields
  {
    F_host_cr0           = 0x6c00,
    F_host_cr3           = 0x6c02,
    F_host_cr4           = 0x6c04,
    F_host_fs_base       = 0x6c06,
    F_host_gs_base       = 0x6c08,
    F_host_tr_base       = 0x6c0a,
    F_host_gdtr_base     = 0x6c0c,
    F_host_idtr_base     = 0x6c0e,
    F_host_sysenter_esp  = 0x6c10,
    F_host_sysenter_eip  = 0x6c12,
    F_host_rip           = 0x6c16,
  };

  static Per_cpu<Vmx> cpus;
  Vmx_info info;

  template< typename T >
  static T vmread(Mword field)
  {
    if (sizeof(T) <= sizeof(Mword))
      return vmread_insn(field);

    return vmread_insn(field) | ((Unsigned64)vmread_insn(field + 1) << 32);
  }

  template< typename T >
  static void vmwrite(Mword field, T value)
  {
    Mword err = vmwrite_insn(field, (Mword)value);
    if (EXPECT_FALSE(err & 0x1))
      WARNX(Info, "VMX: VMfailInvalid vmwrite(0x%04lx, %llx) => %lx\n",
            field, (Unsigned64)value, err);
    else if (EXPECT_FALSE(err & 0x40))
      WARNX(Info, "VMX: VMfailValid vmwrite(0x%04lx, %llx) => %lx, insn error: 0x%x\n",
            field, (Unsigned64)value, err, vmread<Unsigned32>(F_vm_instruction_error));
    if (sizeof(T) > sizeof(Mword))
      vmwrite_insn(field + 1, ((Unsigned64)value >> 32));
  }

  Vmx(Cpu_number cpu);

  void pm_on_resume(Cpu_number cpu) override;
  void pm_on_suspend(Cpu_number cpu) override;

  void init_vmcs_infos(void *vcpu_state) const
  {
    Vmx_user_info *i = reinterpret_cast<Vmx_user_info*>((char*)vcpu_state + 0x200);
    i->basic = info.basic;
    i->pinbased = info.pinbased_ctls;
    i->procbased = info.procbased_ctls;
    i->exit = info.exit_ctls;
    // relax the IRQ on exit for the VMM. Nevertheless,
    // our API hides this feature completely from the VMM
    i->exit.relax(Vmx_info::Ex_ack_irq_on_exit);
    i->entry = info.entry_ctls;
    i->misc = info.misc;
    i->cr0_or = info.cr0_defs.must_be_one();
    i->cr0_and = info.cr0_defs.may_be_one();
    i->cr4_or = info.cr4_defs.must_be_one();
    i->cr4_and = info.cr4_defs.may_be_one();
    i->vmcs_field_info = info.max_index;
    i->procbased2 = info.procbased_ctls2;
    i->ept_vpid_cap = info.ept_vpid_cap;
    i->pinbased_dfl1 = info.pinbased_ctls_default1;
    i->procbased_dfl1 = info.procbased_ctls_default1;
    i->exit_dfl1 = info.exit_ctls_default1;
    i->entry_dfl1 = info.entry_ctls_default1;

    Vmx_user_info::Fo_table *infos = reinterpret_cast<Vmx_user_info::Fo_table *>((char*)vcpu_state + 0x420);
    Unsigned32 *inf = reinterpret_cast<Unsigned32 *>((char*)vcpu_state + 0x410);
    inf[0] = F_sw_guest_cr2;
    infos->init();
  }

  void *kernel_vmcs() const
  { return _kernel_vmcs; }

  Address kernel_vmcs_pa() const
  { return _kernel_vmcs_pa; }

  bool vmx_enabled() const
  { return _vmx_enabled; }

  bool has_vpid() const
  { return _has_vpid; }

private:
  void *_vmxon;
  bool _vmx_enabled;
  bool _has_vpid;
  Unsigned64 _vmxon_base_pa;
  void *_kernel_vmcs;
  Unsigned64 _kernel_vmcs_pa;

  static Mword vmread_insn(Mword field)
  {
    Mword val;
    asm volatile("vmread %1, %0" : "=r" (val) : "r" (field) : "cc");
    return val;
  }

  static Mword vmwrite_insn(Mword field, Mword value)
  {
    Mword err;
    asm volatile("vmwrite %1, %2  \n\t"
                 "pushf           \n\t"
                 "pop %0          \n\t"
                 : "=r" (err) : "r" ((Mword)value), "r" (field) : "cc");
    return err;
  }

  bool handle_bios_lock();
};

class Vmx_info_msr
{
private:
  Unsigned64 _data;
};

