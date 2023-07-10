
#include <fpu.h>
#include <cpu.h>
#include <globalconfig.h>

#include <cstdio>

#ifdef CONFIG_LAZY_FPU
inline void finish_init(Fpu &f)
{
  f.set_owner(nullptr);
  f.disable();
}
#else // CONFIG_LAZY_FPU
inline void finish_init(Fpu &f)
{
  f.enable();
}
#endif // CONFIG_LAZY_FPU


Fpu_arch::Variants Fpu_arch::_variant;
unsigned Fpu_arch::_state_size;
unsigned Fpu_arch::_state_align;

static void init_xsave(Cpu_number cpu)
{
  Unsigned32 eax, ebx, ecx, edx;

  Cpu::cpus.cpu(cpu).cpuid(0xd, 0, &eax, &ebx, &ecx, &edx);

  // allow safe features: x87, SSE, AVX and AVX512
  Fpu::fpu.cpu(cpu).set_xcr0_shadow((((Unsigned64)edx << 32) | eax) & Cpu::Xstate_defined_bits);

  // enable xsave features
  Cpu::cpus.cpu(cpu).set_cr4(Cpu::cpus.cpu(cpu).get_cr4() | CR4_OSXSAVE);
  Cpu::xsetbv(Fpu::fpu.cpu(cpu).get_xcr0(), 0);
}

static void init_disable()
{
  // disable Coprocessor Emulation to allow exception #7/NM on TS
  // enable Numeric Error (exception #16/MF, native FPU mode)
  // enable Monitor Coprocessor
  Cpu::set_cr0((Cpu::get_cr0() & ~CR0_EM) | CR0_NE | CR0_MP);
}

void FIASCO_INIT_CPU_AND_PM
Fpu_arch::init(Cpu_number cpu, bool resume)
{
  // At first, noone owns the FPU
  Fpu &f = Fpu::fpu.cpu(cpu);

  init_disable();

  if (cpu == Cpu_number::boot_cpu() && !resume)
    printf("FPU%u: %s%s\n", cxx::int_value<Cpu_number>(cpu),
           Cpu::cpus.cpu(cpu).features() & FEAT_SSE  ? "SSE "  : "",
           Cpu::cpus.cpu(cpu).ext_features() & FEATX_AVX ? "AVX "  : "");

  unsigned cpu_align = 0, cpu_size  = 0;

  if (Cpu::cpus.cpu(cpu).has_xsave())
    {
      init_xsave(cpu);

      Cpu::cpus.cpu(cpu).update_features_info();

      Unsigned32 eax, ecx, edx;
      Cpu::cpus.cpu(cpu).cpuid(0xd, 0, &eax, &cpu_size, &ecx, &edx);
      cpu_align = 64;
      f._variant = Variant_xsave;
    }
  else if (Cpu::have_fxsr())
    {
      cpu_size  = sizeof(sse_regs);
      cpu_align = 16;
      f._variant  = Variant_fxsr;
    }
  else
    {
      cpu_size  = sizeof(fpu_regs);
      cpu_align = 4;
      f._variant  = Variant_fpu;
    }

  if (cpu_size > _state_size)
    _state_size = cpu_size;
  if (cpu_align > _state_align)
    _state_align = cpu_align;

  finish_init(f);
}

