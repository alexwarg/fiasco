
#include <types.h>
#include <io.h>
#include <mem_layout.h>
#include <mmio_register_block.h>
#include <pfc-arm.h>
#include <globalconfig.h>
#include <poll_timeout_kclock.h>
#include <outer_cache.h>
#include <kmem.h>
#include <infinite_loop.h>
#include <platform.h>

#include <scheduler_iface.h>
#include <sched.h>
#include <cpu.h>
#include <cstdio>


struct Pfc_exynos_base : Pfc_arm
{
public:
  Pfc_exynos_base() : pmu(Kmem::mmio_remap(Mem_layout::Pmu_phys_base, 0x4000))
  {}

  [[noreturn]] void system_reboot() override
  {
    pmu.write(1, 0x400);

    // we should reboot now
    L4::infinite_loop();
  }

protected:
  class Pmu : public Mmio_register_block
  {
  public:
    explicit Pmu(Address virt) : Mmio_register_block(virt) {}
    enum Reg
    {
      Config      = 0,
      Status      = 4,
      Option      = 8,
      Core_offset = 0x80,

      ARM_COMMON_OPTION      = 0x2408,
   };

    static Mword core_pwr_reg(Cpu_phys_id cpu, unsigned func)
    { return 0x2000 + Core_offset * cxx::int_value<Cpu_phys_id>(cpu) + func; }

    Mword read(unsigned reg) const
    { return Mmio_register_block::read<Mword>(reg); }

    void write(Mword val, unsigned reg) const
    { Mmio_register_block::write(val, reg); }

    Mword core_read(Cpu_phys_id cpu, unsigned reg) const
    { return Mmio_register_block::read<Mword>(core_pwr_reg(cpu, reg)); }

    void core_write(Mword val, Cpu_phys_id cpu, unsigned reg) const
    { Mmio_register_block::write(val, core_pwr_reg(cpu, reg)); }
  };

  enum Power_down_mode
  {
    Aftr, Lpa, Dstop, Sleep,

    Pwr_down_mode = Aftr,
  };

  enum Pmu_regs
  {
    Pmu_core_local_power_enable = 3,
  };

  static void write_phys_mem_coherent(Mword addr_p, Mword value)
  {
    Mword addr_v = Kmem::mmio_remap(addr_p, sizeof(Mword));
    Io::write<Mword>(value, addr_v);
    Mem_unit::flush_dcache((void *)addr_v, (void *)(addr_v + sizeof(value)));
    Outer_cache::flush(addr_p);
  }

  void cpuboot(Mword startup_vector, Cpu_phys_id cpu);
  void send_boot_ipi(unsigned val);
  void do_sleep();

  Pmu pmu;
};

#ifdef CONFIG_MP

#ifdef CONFIG_CPU_SUSPEND
class Pfc_exynos_mp : public Pfc_exynos_base
{
private:

public:
  bool power_down_cpu(Cpu_number cpu) override
  {
    if (!_suspend_allowed.get(cpu))
      return false;

    Sched<>::take_cpu_offline(cpu);
    Scheduler_iface::root()->trigger_hotplug_event();

    do_core_n_off(cpu);

    Context::take_cpu_online(cpu);
    Scheduler_iface::root()->trigger_hotplug_event();
    return true;
  }

  void do_boot_ap_cpus(Address phys_reset_vector) override;

  int cpu_allow_shutdown(Cpu_number cpu, bool allow) override
  {
    if (!Cpu::present_mask().get(cpu))
      return -L4_err::ENodev;

    if (allow && Cpu::online(cpu))
      suspend_cpu(cpu);
    else if (!allow && !Cpu::online(cpu))
      resume_cpu(cpu);

    return 0;
  }

  void init(Cpu_number cpu) override
  {
    if (cpu != Cpu_number::boot_cpu())
      return;

    for (Cpu_phys_id i = Cpu_phys_id(0);
         i < Cpu_phys_id(2);
         ++i)
      pmu.core_write((pmu.core_read(i, Pmu::Option) & ~(1 << 0)) | (1 << 1), i, Pmu::Option);

    pmu.write(2, Pmu::ARM_COMMON_OPTION);
  }

protected:
  int power_up_core(Cpu_phys_id cpu)
  {
    // CPU already powered up?
    if ((pmu.core_read(cpu, Pmu::Status) & Pmu_core_local_power_enable) != 0)
      return 0;

    pmu.core_write(Pmu_core_local_power_enable, cpu, Pmu::Config);

    Lock_guard<Cpu_lock, Lock_guard_inverse_policy> cpu_lock_guard(&cpu_lock);

    Poll_timeout_kclock pt(10000);
    while (pt.test((pmu.core_read(cpu, Pmu::Status)
                    & Pmu_core_local_power_enable)
                   != Pmu_core_local_power_enable))
        ;

    return pt.timed_out() ? -L4_err::ENodev : 0;
  }

  int do_core_n_off(Cpu_number cpu)
  {
    if (cpu == Cpu_number::boot_cpu())
      return -L4_err::EBusy;

    Cpu_phys_id const phys_cpu = Cpu::cpus.cpu(cpu).phys_id();

    do_print_cpu_info(phys_cpu);

    assert(cpu_lock.test()); // required for wfi

    pmu.core_write(0, phys_cpu, Pmu::Config);

    Mem_unit::flush_cache();
    Mem_unit::tlb_flush();
    Mem_unit::kernel_tlb_flush();

    Cpu::disable_smp();
    Cpu::disable_dcache();

    do_sleep();

    // we only reach here if the wfi was not done due to a pending event

    Cpu::enable_dcache();
    Cpu::enable_smp();

    // todo: the timer irq needs a proper cpu setting here too
    // (save + restore state)
    Pic::reinit(cpu);

    do_print_cpu_info(phys_cpu);

    return 0;
  }

private:
  Cpu_mask _suspend_allowed;

  void do_print_cpu_info(Cpu_phys_id cpu)
  {
    printf("fiasco: core%d: %lx/%lx/%lx\n", cxx::int_value<Cpu_phys_id>(cpu),
           pmu.core_read(cpu, Pmu::Config),
           pmu.core_read(cpu, Pmu::Status),
           pmu.core_read(cpu, Pmu::Option));
  }

  int resume_cpu(Cpu_number cpu)
  {
    Cpu_phys_id const pcpu = Cpu::cpus.cpu(cpu).phys_id();
    int r;
    if ((r = power_up_core(pcpu)))
      return r;

    _suspend_allowed.atomic_clear(cpu);
    extern char _tramp_mp_entry[];
    cpuboot(Kmem::kdir->virt_to_phys((Address)_tramp_mp_entry), pcpu);
    Ipi::send(Ipi::Global_request, current_cpu(), cpu);

    return 0;
  }

  int suspend_cpu(Cpu_number cpu)
  {
    _suspend_allowed.atomic_set(cpu);
    Ipi::send(Ipi::Global_request, current_cpu(), cpu);
    return 0;
  }
};

#else // CONFIG_CPU_SUSPEND
class Pfc_exynos_mp : public Pfc_exynos_base
{
public:
  int power_up_core(Cpu_phys_id)
  {
    return 0;
  }

  void do_boot_ap_cpus(Address phys_reset_vector) override;
};

#endif // CONFIG_CPU_SUSPEND

#else // CONFIG_MP

class Pfc_exynos_mp : public Pfc_exynos_base
{
public:
  int power_up_core(Cpu_phys_id)
  {
    return -L4_err::ENodev;
  }
};

#endif // CONFIG_MP

#ifdef CONFIG_ARM_SECMONIF_MC

#include <exynos_smc.h>

void
Pfc_exynos_base::cpuboot(Mword startup_vector, Cpu_phys_id cpu)
{
  unsigned long b = Mem_layout::Sysram_phys_base;
  if (Platform::is_5410())
    b += 0x5301c;
  else
    b += Platform::is_4210() ? 0x1f01c : 0x2f01c;

  if (Platform::is_4412())
    b += cxx::int_value<Cpu_phys_id>(cpu) * 4;

  write_phys_mem_coherent(b, startup_vector);
  Exynos_smc::call(Exynos_smc::Cpu1boot, cxx::int_value<Cpu_phys_id>(cpu));
}

void
Pfc_exynos_base::do_sleep()
{
  // FIXME: I miss: Exynos_smc::cpusleep();
  asm volatile("dsb; wfi" : : : "memory", "cc");
}

#else

void
Pfc_exynos_base::cpuboot(Mword startup_vector, Cpu_phys_id cpu)
{
 unsigned long b = Mem_layout::Sysram_phys_base;

  if (Platform::is_4210() && Platform::subrev() == 0x11)
    b = Mem_layout::Pmu_phys_base + 0x814;
  else if (Platform::is_4210() && Platform::subrev() == 0)
    b += 0x5000;

  if (Platform::is_4412())
    b += cxx::int_value<Cpu_phys_id>(cpu) * 4;

  write_phys_mem_coherent(b, startup_vector);
}

void
Pfc_exynos_base::do_sleep()
{
  asm volatile("dsb; wfi" : : : "memory", "cc");
}

#endif

#ifdef CONFIG_MP
#ifdef CONFIG_PF_EXYNOS_EXTGIC

void
Pfc_exynos_base::send_boot_ipi(unsigned val)
{
  Pic::gic.current()->softint_phys(Ipi::Global_request, 1u << (16 + val));
}
#else // CONFIG_PF_EXYNOS_EXTGIC

void
Pfc_exynos_base::send_boot_ipi(unsigned val)
{
  Pic::gic->softint_phys(Ipi::Global_request, 1u << (16 + val));
}

#endif // CONFIG_PF_EXYNOS_EXTGIC

void
Pfc_exynos_mp::do_boot_ap_cpus(Address phys_reset_vector)
{
  assert(current_cpu() == Cpu_number::boot_cpu());

  if (Platform::is_4412() || Platform::is_5410())
    {
      for (unsigned i = 1;
           i < 4 && i < Config::Max_num_cpus;
           ++i)
        {
          power_up_core(Cpu_phys_id(i));
          if (Platform::is_4412())
            cpuboot(phys_reset_vector, Cpu_phys_id(i));
          send_boot_ipi(i);
        }

      return;
    }

  unsigned const second = 1;
  power_up_core(Cpu_phys_id(second));
  cpuboot(phys_reset_vector, Cpu_phys_id(second));
  send_boot_ipi(second);
}
#endif // CONFIG_MP


static Pfc_singleton<Pfc_exynos_mp> __pfc;
