#pragma once

#include <types.h>

/**
 * Encapsulation of the platforms interrupt controller
 */
class Pic
{
public:
  static void init();
  static void init_ap(Cpu_number cpu, bool resume);
};

