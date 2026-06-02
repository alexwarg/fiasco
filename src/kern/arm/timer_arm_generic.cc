#include <timer_arm_generic.h>
#include <cpu.h>
#include <arch_time_source-generic_timer.h>

#include <panic.h>
#include <cstdio>

Mword Timer_generic_timer::_interval;
Mword Timer_generic_timer::_freq0;

bool
Timer_generic_timer::check_and_disable(Cpu_number cpu)
{
  if (!Cpu::cpus.cpu(cpu).has_generic_timer())
    panic("CPU does not support the ARM generic timer");

  if (Proc::Is_hyp)
    Generic_timer::T<Generic_timer::Virtual>::control(0);

  Gtimer::control(0);

  if (is_boot_cpu(cpu))
    _freq0 = Gtimer::frequency();

  return true;
}

void
Timer_generic_timer::finalize_init(Cpu_number cpu)
{
  if (is_boot_cpu(cpu))
    {
      _interval = Unsigned64{_freq0} * Config::Scheduler_granularity / 1000000;
      printf("ARM generic timer: freq=%ld interval=%ld cnt=%lld\n", _freq0, _interval, Gtimer::counter());
      assert(_freq0);

      Arch_time_source_generic_timer::setup_scalers(_freq0);
    }
  else if (_freq0 !=  Gtimer::frequency())
    {
      printf("Different frequency on AP CPUs");
      Gtimer::frequency(_freq0);
    }

  Gtimer::setup_timer_access();

  // wait for timer to really start counting
  Unsigned64 v = Gtimer::counter();
  while (Gtimer::counter() == v)
    ;
}


