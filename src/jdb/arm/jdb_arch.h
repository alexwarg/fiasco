#pragma once

#include <globalconfig.h>
#include <kernel_uart.h>

class Jdb_arm_base
{
public:
  static int is_adapter_memory(Jdb_address) { return 0; }
  static int (*bp_test_log_only)(Cpu_number);
  static int (*bp_test_break)(Cpu_number, String_buffer *);

  static bool test_checksums() { return true; }
  static bool handle_special_cmds(int) { return true; }

#ifndef CONFIG_ARM_GIC
  static void wfi_enter() {}
  static void wfi_leave() {}
#else
  static void wfi_enter();
  static void wfi_leave();
#  ifdef CONFIG_SERIAL
  static void kernel_uart_irq_ack()
  { Kernel_uart::uart()->irq_ack(); }
#  else
  static void kernel_uart_irq_ack() {}
#  endif
#endif
};

using Jdb_arch = Jdb_arm_base;
