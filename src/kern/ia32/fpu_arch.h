#pragma once

#include <cpu.h>
#include <fpu_state_ptr.h>
#include <regdefs.h>

#include <std_macros.h>
#include <globalconfig.h>
#include <cstring>

class Fpu_arch
{
public:
  enum Variants
  {
    Variant_fpu,
    Variant_fxsr,
    Variant_xsave,
  };

private:
  struct fpu_regs       // saved FPU registers
  {
    long    cwd;
    long    swd;
    long    twd;
    long    fip;
    long    fcs;
    long    foo;
    long    fos;
    long    st_space[20];   /* 8*10 bytes for each FP-reg = 80 bytes */
  };

  struct sse_regs
  {
    Unsigned16 cwd;
    Unsigned16 swd;
    Unsigned16 twd;
    Unsigned16 fop;
    Unsigned32 fip;
    Unsigned32 fcs;
    Unsigned32 foo;
    Unsigned32 fos;
    Unsigned32 mxcsr;
    Unsigned32 reserved;
    Unsigned32 st_space[32];   /*  8*16 bytes for each FP-reg  = 128 bytes */
    Unsigned32 xmm_space[64];  /* 16*16 bytes for each XMM-reg = 256 bytes */
    Unsigned32 padding[24];
  };

  struct Xsave_buffer
  {
    sse_regs sse;
    Unsigned64 header[8];
  };

  Unsigned64 _xcr0;

  static Variants _variant;
  static unsigned _state_size;
  static unsigned _state_align;

public:
  static unsigned state_size()
  {
    return _state_size;
  }

  static unsigned state_align()
  {
    return _state_align;
  }

  Unsigned64 get_xcr0() const
  {
    return _xcr0;
  }

  void set_xcr0_shadow(Unsigned64 x)
  {
    _xcr0 = x;
  }

  /*
   * Mark the FPU busy. The next attempt to use it will yield a trap.
   */
  static void disable()
  {
    Cpu::set_cr0(Cpu::get_cr0() | CR0_TS);
  }

  /*
   * Mark the FPU no longer busy. Subsequent FPU access won't trap.
   */
  static void enable()
  {
    asm volatile ("clts");
  }

  static void init(Cpu_number cpu, bool resume);

  /*
   * Initialize FPU or SSE state
   * We don't use finit, because it is slow. Initializing the context in
   * memory and fetching it via restore_state is supposedly faster
   */
  static void init_state(Fpu_state *s)
  {
    Cpu const &_cpu = *Cpu::boot_cpu();
    if (_cpu.features() & FEAT_FXSR)
      {
        sse_regs *sse = reinterpret_cast<sse_regs *>(s);

        memset(sse, 0, sizeof (*sse));
        sse->cwd = 0x37f;

        if (_cpu.features() & FEAT_SSE)
          sse->mxcsr = 0x1f80;

        if (_cpu.has_xsave())
          memset(reinterpret_cast<Xsave_buffer *>(s)->header, 0,
                 sizeof (Xsave_buffer::header));

        static_assert(sizeof (sse_regs) == 512, "SSE-regs size not 512 bytes");
      }
    else
      {
        fpu_regs *fpu = reinterpret_cast<fpu_regs *>(s);

        memset(fpu, 0, sizeof (*fpu));
        fpu->cwd = 0xffff037f;
        fpu->swd = 0xffff0000;
        fpu->twd = 0xffffffff;
        fpu->fos = 0xffff0000;
      }
  }

  static void save_state(Fpu_state *s)
  {
    assert (s);

    // Both fxsave and fnsave are non-waiting instructions and thus
    // cannot cause exception #16 for pending FPU exceptions.

    switch (_variant)
      {
      case Variant_xsave:
        asm volatile("xsave (%2)" : : "a" (~0UL), "d" (~0UL), "r" (s) : "memory");
        break;
      case Variant_fxsr:
        asm volatile ("fxsave (%0)" : : "r" (s) : "memory");
        break;
      case Variant_fpu:
        asm volatile ("fnsave (%0)" : : "r" (s) : "memory");
        break;
      }
  }

  /*
   * Restore a saved FPU or SSE state
   */
  static void restore_state(Fpu_state const *s, bool owner)
  {
    assert (s);

    switch (_variant)
      {
      case Variant_xsave:
        asm volatile ("xrstor (%2)" : : "a" (~0UL), "d" (~0UL), "r" (s));
        break;
      case Variant_fxsr:
          {
#if !defined (CONFIG_WORKAROUND_AMD_FPU_LEAK)
            asm volatile ("fxrstor (%0)" : : "r" (s));
#else
            /* The code below fixes a security leak on AMD CPUs, where
             * some registers of the FPU are not restored from the state_buffer
             * if there are no FPU exceptions pending. The old values, from the
             * last FPU owner, are therefore leaked to the new FPU owner.
             */
            static Mword int_dummy = 0;

            asm volatile(
                "fnstsw	%%ax    \n\t"   // save fpu flags in ax
                "ffree	%%st(7) \n\t"   // make enough space for the fildl
                "bt       $7,%%ax \n\t"   // test if exception bit is set
                "jnc      1f      \n\t"
                "fnclex           \n\t"   // clear it
                "1: fildl %1      \n\t"   // dummy load which sets the
                // affected to def. values
                "fxrstor (%0)     \n\t"   // finally restore the state
                : : "r" (s), "m" (int_dummy) : "ax");
#endif
          }
        break;
      case Variant_fpu:
        // this should be handled in the cases where we release the FPU and it has no owner anymore...

        // frstor is a waiting instruction and we must make sure no
        // FPU exceptions are pending here. We distinguish two cases:
        // 1) If we had a previous FPU owner, we called save_state before and
        //    invoked fnsave which re-initialized the FPU and cleared exceptions
        // 2) Otherwise we call fnclex instead to clear exceptions.
        if (IS_ENABLED(CONFIG_LAZY_FPU) && !owner)
          asm volatile ("fnclex");

        asm volatile ("frstor (%0)" : : "r" (s));
        break;
      }
  }
};

