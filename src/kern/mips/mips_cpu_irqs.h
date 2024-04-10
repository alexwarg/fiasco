#pragma once

class Irq_chip_icu;

class Mips_cpu_irqs
{
public:
  static Irq_chip_icu *chip;
  static void init();
};

