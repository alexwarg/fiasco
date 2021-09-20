
#include <fpu.h>
#include <globalconfig.h>

#include <cstdio>

#ifdef CONFIG_CPU_MIPSR6
  [[gnu::always_inline]]
  inline void set_mipsr2_fp64()
  {
    asm volatile(".macro set_mipsr2_fp64\n"
                 ".endm                 \n");
  }

#else // CONFIG_CPU_MIPSR6
#ifdef CONFIG_BIT32
  [[gnu::always_inline]]
  inline void set_mipsr2_fp64()
  {
    asm volatile(".macro set_mipsr2_fp64\n"
                 "        .set mips32r2 \n"
                 "        .set fp=64    \n"
                 ".endm                 \n");
  }
#endif // CONFIG_BIT32
#ifdef CONFIG_BIT64
  [[gnu::always_inline]]
  inline void set_mipsr2_fp64()
  {
    asm volatile(".macro set_mipsr2_fp64\n"
                 "        .set mips64r2 \n"
                 "        .set fp=64    \n"
                 ".endm                 \n");
  }
#endif // CONFIG_BIT64
#endif // CONFIG_CPU_MIPSR6

#ifdef CONFIG_LAZY_FPU

inline void finish_init(Fpu &f)
{
  f.disable();
  f.set_owner(0);
}

#else // CONFIG_LAZY_FPU

inline void finish_init(Fpu &)
{}

#endif // CONFIG_LAZY_FPU

void
Fpu_arch::show(Cpu_number cpu)
{
  const Fir f = Fpu::fpu.cpu(cpu).fir();

  printf("FPU[%d]: fir:%08x ID:%x Rev:%x fp-type%s%s%s%s%s F64:%x "
         "UFRP:%x FREP:%x\n",
         cxx::int_value<Cpu_number>(cpu),
         (int)f.v,
         (int)f.processor_id(),
         (int)f.revision(),
         f.s() ? ":S" : "",
         f.d() ? ":D" : "",
         f.ps() ? ":PS" : "",
         f.w() ? ":W" : "",
         f.l() ? ":L" : "",
         (int)f.f64(), (int)f.ufrp(), (int)f.frep());
}

void
Fpu_arch::init(Cpu_number cpu, bool resume)
{
  Fpu &f = Fpu::fpu.cpu(cpu);

  f.enable();
  f._fir = Fir(fir_read());

  if (!resume)
    show(cpu);

  finish_init(f);
}

inline void
fpu_save_16even(Fpu_arch::Fpu_regs *r)
{
  Mword tmp;

  asm volatile(".set   push                   \n"
               ".set   hardfloat              \n"
               "cfc1   %[tmp],   $31          \n"
               "sw     %[tmp],   %[fcsr]      \n"
               "sdc1   $f0,  (8 *  0)(%[regs])\n"
               "sdc1   $f2,  (8 *  2)(%[regs])\n"
               "sdc1   $f4,  (8 *  4)(%[regs])\n"
               "sdc1   $f6,  (8 *  6)(%[regs])\n"
               "sdc1   $f8,  (8 *  8)(%[regs])\n"
               "sdc1   $f10, (8 * 10)(%[regs])\n"
               "sdc1   $f12, (8 * 12)(%[regs])\n"
               "sdc1   $f14, (8 * 14)(%[regs])\n"
               "sdc1   $f16, (8 * 16)(%[regs])\n"
               "sdc1   $f18, (8 * 18)(%[regs])\n"
               "sdc1   $f20, (8 * 20)(%[regs])\n"
               "sdc1   $f22, (8 * 22)(%[regs])\n"
               "sdc1   $f24, (8 * 24)(%[regs])\n"
               "sdc1   $f26, (8 * 26)(%[regs])\n"
               "sdc1   $f28, (8 * 28)(%[regs])\n"
               "sdc1   $f30, (8 * 30)(%[regs])\n"
               ".set   pop                    \n"
               : [fcsr] "=m"  (r->fcsr),
                 [tmp]  "=&r" (tmp)
               : [regs] "r"   (r->regs));
}

inline void
fpu_save_16odd(Fpu_arch::Fpu_regs *r)
{
  set_mipsr2_fp64();
  asm volatile(".set   push                   \n"
               ".set   hardfloat              \n"
               "set_mipsr2_fp64               \n"
               ".purgem set_mipsr2_fp64       \n"
               "sdc1   $f1,  (8 *  1)(%[regs])\n"
               "sdc1   $f3,  (8 *  3)(%[regs])\n"
               "sdc1   $f5,  (8 *  5)(%[regs])\n"
               "sdc1   $f7,  (8 *  7)(%[regs])\n"
               "sdc1   $f9,  (8 *  9)(%[regs])\n"
               "sdc1   $f11, (8 * 11)(%[regs])\n"
               "sdc1   $f13, (8 * 13)(%[regs])\n"
               "sdc1   $f15, (8 * 15)(%[regs])\n"
               "sdc1   $f17, (8 * 17)(%[regs])\n"
               "sdc1   $f19, (8 * 19)(%[regs])\n"
               "sdc1   $f21, (8 * 21)(%[regs])\n"
               "sdc1   $f23, (8 * 23)(%[regs])\n"
               "sdc1   $f25, (8 * 25)(%[regs])\n"
               "sdc1   $f27, (8 * 27)(%[regs])\n"
               "sdc1   $f29, (8 * 29)(%[regs])\n"
               "sdc1   $f31, (8 * 31)(%[regs])\n"
               ".set   pop                    \n"
               :: [regs] "r" (r->regs));
}

inline void
fpu_restore_16even(Fpu_arch::Fpu_regs const *r)
{
  Mword tmp;

  asm volatile(".set   push                   \n"
               ".set   hardfloat              \n"
               "ldc1   $f0,  (8 *  0)(%[regs])\n"
               "ldc1   $f2,  (8 *  2)(%[regs])\n"
               "ldc1   $f4,  (8 *  4)(%[regs])\n"
               "ldc1   $f6,  (8 *  6)(%[regs])\n"
               "ldc1   $f8,  (8 *  8)(%[regs])\n"
               "ldc1   $f10, (8 * 10)(%[regs])\n"
               "ldc1   $f12, (8 * 12)(%[regs])\n"
               "ldc1   $f14, (8 * 14)(%[regs])\n"
               "ldc1   $f16, (8 * 16)(%[regs])\n"
               "ldc1   $f18, (8 * 18)(%[regs])\n"
               "ldc1   $f20, (8 * 20)(%[regs])\n"
               "ldc1   $f22, (8 * 22)(%[regs])\n"
               "ldc1   $f24, (8 * 24)(%[regs])\n"
               "ldc1   $f26, (8 * 26)(%[regs])\n"
               "ldc1   $f28, (8 * 28)(%[regs])\n"
               "ldc1   $f30, (8 * 30)(%[regs])\n"
               "lw     %[tmp], %[fcsr]        \n"
               "ctc1   %[tmp], $31            \n"
               ".set   pop                    \n"
               : [tmp]  "=&r" (tmp)
               : [regs] "r"   (r->regs),
                 [fcsr] "m"   (r->fcsr));
}

inline void
fpu_restore_16odd(Fpu_arch::Fpu_regs const *r)
{
  set_mipsr2_fp64();
  asm volatile(".set   push                   \n"
               ".set   hardfloat              \n"
               "set_mipsr2_fp64               \n"
               ".purgem set_mipsr2_fp64       \n"
               "ldc1   $f1,  (8 *  1)(%[regs])\n"
               "ldc1   $f3,  (8 *  3)(%[regs])\n"
               "ldc1   $f5,  (8 *  5)(%[regs])\n"
               "ldc1   $f7,  (8 *  7)(%[regs])\n"
               "ldc1   $f9,  (8 *  9)(%[regs])\n"
               "ldc1   $f11, (8 * 11)(%[regs])\n"
               "ldc1   $f13, (8 * 13)(%[regs])\n"
               "ldc1   $f15, (8 * 15)(%[regs])\n"
               "ldc1   $f17, (8 * 17)(%[regs])\n"
               "ldc1   $f19, (8 * 19)(%[regs])\n"
               "ldc1   $f21, (8 * 21)(%[regs])\n"
               "ldc1   $f23, (8 * 23)(%[regs])\n"
               "ldc1   $f25, (8 * 25)(%[regs])\n"
               "ldc1   $f27, (8 * 27)(%[regs])\n"
               "ldc1   $f29, (8 * 29)(%[regs])\n"
               "ldc1   $f31, (8 * 31)(%[regs])\n"
               ".set   pop                    \n"
               :: [regs] "r" (r->regs));
}

void
Fpu_arch::save_state(Fpu_state *fpu_regs)
{
  assert(fpu_regs);

  if (Fpu::mode_64bit())
    fpu_save_16odd(fpu_regs);

  fpu_save_16even(fpu_regs);
}

void
Fpu_arch::restore_state(Fpu_state const *fpu_regs, bool)
{
  assert(fpu_regs);

  if (Fpu::mode_64bit())
    fpu_restore_16odd(fpu_regs);

  fpu_restore_16even(fpu_regs);
}

