#include <cpu_tickless_idle.h>

DEFINE_PER_CPU Per_cpu<unsigned long> Cpu_tickless_idle_base::idle_counter;
DEFINE_PER_CPU Per_cpu<unsigned long> Cpu_tickless_idle_base::deep_idle_counter;

