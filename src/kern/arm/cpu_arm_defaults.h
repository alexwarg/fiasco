#pragma once

class Cpu_arm_defaults
{
public:
  static void init_errata_workarounds() {}
  static void init_supervisor_mode(bool) {}
  static void init_hyp_mode(bool /* is_boot_cpu */) {}
  static void init_tz() {}
  void id_init() {}
  static void enable_smp() noexcept {}
};
