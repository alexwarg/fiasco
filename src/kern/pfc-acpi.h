#pragma once

#include <pfc.h>
#include <types.h>
#include <acpi.h>
#include <acpi_fadt.h>
#include <cpu_call.h>
#include <context.h>
#include <fpu.h>
#include <timer_tick.h>
#include <kernel_thread.h>
#include <pm.h>

/* implemented in ia32/tramp-acpi.S */
extern "C" FIASCO_FASTCALL
int acpi_save_cpu_and_suspend(Unsigned32 sleep_type,
                              Unsigned32 pm1_cntl,
                              Unsigned32 pm1_sts);


template<typename Base>
class Pfc_acpi : public virtual Pfc, Base
{
private:
  struct Suspend_data
  {
    Pfc_acpi *pfc;
    Mword extra;
    int res = 0;
    Suspend_data(Pfc_acpi *pfc, Mword extra): pfc(pfc), extra(extra) {}
  };

public:
  void init(Cpu_number cpu) override
  {
    if (cpu != Cpu_number::boot_cpu())
      return;

    Acpi_fadt const *fadt = Acpi::find<Acpi_fadt const *>("FACP");
    if (!fadt)
      {
        printf("ACPI: cannot find FADT, so suspend support disabled\n");
        return;
      }

    facs = Acpi::map_table_head<Acpi_facs>(fadt->facs_addr);
    printf("ACPI: FACS phys=%x virt=%p\n", fadt->facs_addr, facs);

    if (!facs)
      {
        printf("ACPI: cannot map FACS, so suspend support disabled\n");
        return;
      }

    if (!Acpi::check_signature(facs->signature, "FACS"))
      {
        printf("ACPI: FACS signature invalid, so suspend support disabled\n");
        return;
      }

    printf("ACPI: HW sig=%x\n", facs->hw_signature);

    extern char _tramp_acpi_wakeup[];
    phys_wake_vector = reinterpret_cast<Address>(_tramp_acpi_wakeup);
    if (phys_wake_vector >= 1UL << 20)
      {
        printf("ACPI: invalid wake vector (1MB): %lx\n", phys_wake_vector);
        return;
      }

    extern volatile Address _realmode_startup_pdbr;
    _realmode_startup_pdbr = Kmem::get_realmode_startup_pdbr();
    facs->fw_wake_vector = phys_wake_vector;

    // The fadt pointer is only valid in the kernel address space of idle. To
    // avoid dereferencing it in a different context, we cache the values we
    // need.
    _pm1a = fadt->pm1a_cntl_blk;
    _pm1b = fadt->pm1b_cntl_blk;
    _pm1a_sts = fadt->pm1a_evt_blk;
    _pm1b_sts = fadt->pm1b_evt_blk;
    _fadt_reset_value = fadt->reset_value;
    _fadt_reset_regs_addr = fadt->reset_regs.addr;

    _system_suspend_enabled = true;
  }

  int system_suspend(Mword extra) override
  {
    auto guard = lock_guard(cpu_lock);

    if (!_system_suspend_enabled)
      return -L4_err::ENodev;

    Cpu_mask cpus;
    cpus.set(Cpu_number::boot_cpu());

    Suspend_data d(this, extra);
    Cpu_call::cpu_call_many(cpus, [&d](Cpu_number)
      {
        current()->kernel_context_drq(do_system_suspend, &d);
        return false;
      }, true);

    return d.res;
  }

private:
  bool _system_suspend_enabled = false;
  Unsigned32 _pm1a, _pm1b, _pm1a_sts, _pm1b_sts;
  Unsigned32 _fadt_reset_value, _fadt_reset_regs_addr;
  Address phys_wake_vector;
  Acpi_facs *facs;

  static Context::Drq::Result
  do_system_suspend(Context::Drq *, Context *, void *data)
  {
    Suspend_data *d = static_cast<Suspend_data *>(data);
    return d->pfc->_do_system_suspend(d);
  }

  /**
   * \brief Initiate a full system suspend to RAM.
   * \pre must run on the boot CPU
   */
  Context::Drq::Result
  _do_system_suspend(Suspend_data *d)
  {
    assert (current_cpu() == Cpu_number::boot_cpu());
    Context::spill_current_fpu(current_cpu());
    suspend_ap_cpus();

    facs->fw_wake_vector = phys_wake_vector;
    if (facs->len > 32 && facs->version >= 1)
      facs->x_fw_wake_vector = 0;

    Mword sleep_type = d->extra;
    d->res = 0;

    Pm_object::run_on_suspend_hooks(current_cpu());

    current()->spill_user_state();

    Cpu::cpus.current().pm_suspend();


    if (acpi_save_cpu_and_suspend(sleep_type,
                                  (_pm1b << 16) | _pm1a,
                                  (_pm1b_sts << 16) | _pm1a_sts))
      d->res = -L4_err::EInval;

    Cpu::cpus.current().pm_resume();

    // mainly for setting FS base and GS base on AMD64
    // must be done after calling Cpu::pm_resume()
    current()->fill_user_state();

    take_boot_cpu_online();

    Pm_object::run_on_resume_hooks(current_cpu());

    Fpu::init(current_cpu(), true);

    Timer::init(current_cpu());
    Timer_tick::enable(current_cpu());
    boot_ap_cpus();

    return Context::Drq::no_answer_resched();
  }

#ifdef CONFIG_MP
  Cpu_mask _cpus_to_suspend;

  void suspend_ap_cpus()
  {
    // NOTE: This code must not be migrated and is not reentrant!
    _cpus_to_suspend = Cpu::online_mask();
    _cpus_to_suspend.clear(Cpu_number::boot_cpu());

    Cpu_call::cpu_call_many(_cpus_to_suspend, [this](Cpu_number cpu)
      {
        Context::spill_current_fpu(cpu);
        current()->kernel_context_drq([](Context::Drq *, Context *, void *_ths)
          {
            Pfc_acpi *self = static_cast<Pfc_acpi *>(_ths);
            Cpu_number cpun = current_cpu();
            Cpu &cpu = Cpu::cpus.current();
            Pm_object::run_on_suspend_hooks(cpun);
            cpu.pm_suspend();
            check (Sched<>::take_cpu_offline(cpun, true));
            // We assume that Platform_control::cpu_suspend() does never return
            // under any circumstances -- otherwise we'd run with inconsistent
            // state into the scheduler.
            Sched_context::rq.cpu(cpun).schedule_in_progress = 0;
            self->prepare_cpu_suspend(cpun);
            self->_cpus_to_suspend.atomic_clear(current_cpu());
            self->cpu_suspend(cpun);
            return Context::Drq::no_answer_resched();
          }, this);
        return false;
      }, true);

    // Wind up pending Rcu and Drq changes together with all _cpus_to_suspend
    check (Sched<>::take_cpu_offline(current_cpu(), true));

    while (!_cpus_to_suspend.empty())
      {
        Proc::pause();
        asm volatile ("" : "=m" (_cpus_to_suspend));
      }
  }

  static void
  take_boot_cpu_online()
  {
    Context::take_cpu_online(current_cpu());
  }

#else
  static void suspend_ap_cpus() {}
  static void take_boot_cpu_online() {}
#endif

};
