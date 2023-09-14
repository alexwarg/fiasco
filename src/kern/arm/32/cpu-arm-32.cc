
#include "cpu.h"
#include "static_init.h"
#include <kmem.h>
#include <kmem_space.h>
#include <kmem_alloc.h>
#include <ram_quota.h>
#include <globalconfig.h>

#if defined(CONFIG_ARM_MPCORE) || defined(CONFIG_ARM_CORTEX_A9) || defined(CONFIG_ARM_CORTEX_A5)

[[gnu::init_priority(EARLY_INIT_PRIO)]]
Scu Cpu::scu;

#endif

static void modify_actl(Unsigned64 mask, Unsigned64 value) noexcept
{
  Mword actrl;
  asm volatile ("mrc p15, 0, %0, c1, c0, 1" : "=r" (actrl));
  if ((actrl & mask) != value)
    asm volatile ("mcr p15, 0, %0, c1, c0, 1" : : "r" ((actrl & mask) | value));
}

static void modify_cpuectl(Unsigned64 mask, Unsigned64 value) noexcept
{
  Mword ectlh, ectll;
  asm volatile ("mrrc p15, 1, %0, %1, c15" : "=r"(ectll), "=r"(ectlh));
  Unsigned64 ectl = (((Unsigned64)ectlh) << 32) | ectll;
  if ((ectl & mask) != value)
    asm volatile ("mcrr p15, 1, %0, %1, c15" : :
                  "r"((ectll & mask) | value),
                  "r"((ectlh & (mask >> 32)) | (value >> 32)));
}


struct Midr_match
{
  Unsigned32 mask;
  Unsigned32 value;
  Unsigned64 f_mask;
  Unsigned64 f_value;
  void (*func)(Unsigned64 mask, Unsigned64 value);
};

static Midr_match _enable_smp[] =
{
  { 0xff0ffff0, 0x410fc050, 0x41, 0x41, &modify_actl },   // Cortex-A5
  { 0xff0ffff0, 0x410fc070, 0x40, 0x40, &modify_actl },   // Cortex-A7
  { 0xff0ffff0, 0x410fc090, 0x41, 0x41, &modify_actl },   // Cortex-A9
  { 0xff0ffff0, 0x410fc0d0, 0x41, 0x41, &modify_actl },   // Cortex-A12
  { 0xff0ffff0, 0x410fc0e0, 0x41, 0x41, &modify_actl },   // Cortex-A17
  { 0xff0ffff0, 0x410fc0f0, 0x41, 0x41, &modify_actl },   // Cortex-A15
  { 0xff0ffff0, 0x410fd040, 0x40, 0x40, &modify_cpuectl }, // Cortex-A35
  { 0xff0ffff0, 0x410fd030, 0x40, 0x40, &modify_cpuectl }, // Cortex-A53
  { 0xff0ffff0, 0x410fd070, 0x40, 0x40, &modify_cpuectl }, // Cortex-A57
  { 0xff0ffff0, 0x410fd080, 0x40, 0x40, &modify_cpuectl }, // Cortex-A72
};

void
Cpu_arm_v7plus_common::enable_smp()
{
  Unsigned32 m = Cpu::midr();
  for (auto const &e : _enable_smp)
    if ((e.mask & m) == e.value)
      {
        e.func(e.f_mask, e.f_value);
        break;
      }
}

void
Cpu_arm_v7plus_common::disable_smp()
{
  Unsigned32 m = Cpu::midr();
  for (auto const &e : _enable_smp)
    if ((e.mask & m) == e.value)
      {
        e.func(e.f_mask, ~e.f_value & e.f_mask);
        break;
      }
}

#ifndef CONFIG_CPU_VIRT

void
Cpu_arm_bits_generic::init_supervisor_mode(bool is_boot_cpu)
{
  if (!is_boot_cpu)
    return;

  extern char ivt_start;
  // map the interrupt vector table to 0xffff0000
  auto pte = Kmem::kdir->walk(Virt_addr(Kmem_space::Ivt_base),
                              Kpdir::Depth, true,
                              Kmem_alloc::q_allocator(Ram_quota::root));

  pte.set_page(Phys_mem_addr((unsigned long)&ivt_start),
               Page::Attr::kern_global(Page::Rights::RWX()));
  pte.write_back_if(true, Mem_unit::Asid_kernel);
}

#endif // !CONFIG_CPU_VIRT


