#pragma once

#include <cpu-ia32-bits.h>

class Cpu : public Cpu_ia32_bits
{
public:
  Cpu(Cpu_number cpu);
  void init();

  static Per_cpu<Cpu> cpus;
  static Cpu *boot_cpu() { return _boot_cpu; }
  static unsigned long get_features();
  void identify();

  static bool vmx() { return boot_cpu()->ext_features() & FEATX_VMX; }
  static bool svm() { return boot_cpu()->ext_8000_0001_ecx() & FEATA_SVM; }
  static bool has_amd_osvw() { return  boot_cpu()->ext_8000_0001_ecx() & (1<<9); }

  static bool have_superpages() { return boot_cpu()->superpages(); }
  static bool have_sysenter() { return boot_cpu()->sysenter(); }
  static bool have_syscall() { return boot_cpu()->syscall(); }
  static bool have_fxsr() { return boot_cpu()->features() & FEAT_FXSR; }
  static bool have_pge() { return boot_cpu()->features() & FEAT_PGE; }
  static bool have_xsave() { return boot_cpu()->ext_features() & FEATX_XSAVE; }

  Unsigned64 time_us() const
  {
    return tsc_to_us(rdtsc());
  }

  void busy_wait_ns(Unsigned64 ns)
  {
    Unsigned64 stop = rdtsc () + ns_to_tsc(ns);

    while (rdtsc() < stop)
      Proc::pause();
  }

  bool if_show_infos() const;
  void show_cache_tlb_info(const char *indent) const;
  void print_infos() const;
  void print_errata();

  void pm_resume();
  void pm_suspend();

private:
  void calibrate_tsc();

  static Cpu *_boot_cpu;
};
