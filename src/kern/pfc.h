#pragma once

#include <types.h>
#include <l4_types.h>

class Pfc
{
public:
  Pfc() noexcept
  {
    _singleton = this;
  }

  Pfc(Pfc const &) = delete;
  Pfc(Pfc &&) = delete;
  virtual ~Pfc() noexcept = default;
  void operator = (Pfc const &) = delete;
  void operator = (Pfc &&) = delete;

  [[noreturn]] virtual void system_off() = 0;
  [[noreturn]] virtual void system_reboot() = 0;

  virtual int system_suspend(Mword extra_data)
  {
    (void) extra_data;
    return -L4_err::EBusy;
  }

  virtual int hotplug_cpu(Cpu_phys_id id)
  {
    (void) id;
    return -L4_err::ENodev;
  }

  virtual int cpu_allow_shutdown(Cpu_number cpu, bool allow)
  {
    (void) cpu;  (void) allow;
    return -L4_err::ENodev;
  }

  virtual void init(Cpu_number cpu)
  {
    (void) cpu;
  }

  virtual void boot_ap_cpus()
  {}

  virtual bool power_down_cpu(Cpu_number cpu)
  {
    (void) cpu;
    return false;
  }

  static Pfc *get()
  {
    return _singleton;
  }

private:
  static Pfc *_singleton;
};

template<typename PFC>
class Pfc_singleton
{
public:
  Pfc_singleton() noexcept
  {
    pfc.init(Cpu_number::boot_cpu());
  }

private:
  PFC pfc;
};

