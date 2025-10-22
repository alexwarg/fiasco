#pragma once

#include <cxx/bitfield>
#include <mem.h>
#include <fpu_state_ptr.h>
#include <globalconfig.h>
#include <cstring>

class Trap_state;

class Fpu_state
{
public:
  Mword fpexc, fpscr, fpinst, fpinst2;
  Mword state[64];

  Fpu_state()
  {
    static_assert(!(sizeof (*this) % sizeof(Mword)),
                  "Non-mword size of Fpu_regs");
    Mem::memset_mwords(this, 0, sizeof (*this) / sizeof(Mword));
#ifdef CONFIG_CPU_VIRT
    enum { FPEXC_EN   = 1 << 30 };
    fpexc |= FPEXC_EN;
#endif
  }
};

struct Fpu_arch
{
  using Fpu_regs = Fpu_state;
  struct Exception_state_user
  {
    Mword fpexc;
    Mword fpinst;
    Mword fpinst2;
  };

  enum
  {
    FPEXC_FP2V = 1 << 28,
    FPEXC_EN   = 1 << 30,
    FPEXC_EX   = 1 << 31,
  };

  /// Result of emulate_insns().
  enum class Emulate_result
  {
    Unknown,    ///< Instruction unknown / not eligible for emulation.
    Undefined,  ///< Instruction undefined (sub architecture too old).
    Emulated,   ///< Instruction was emulated.
  };

  struct Fpsid
  {
    Mword v;

    Fpsid() = default;
    explicit Fpsid(Mword v) : v(v) {}

    CXX_BITFIELD_MEMBER(0, 3, rev, v);
    CXX_BITFIELD_MEMBER(4, 7, variant, v);
    CXX_BITFIELD_MEMBER(8, 15, part_number, v);
    CXX_BITFIELD_MEMBER(16, 22, sub_arch, v);
    CXX_BITFIELD_MEMBER(16, 19, arch_version, v);
    CXX_BITFIELD_MEMBER(24, 31, implementer, v);
  };

  Fpsid fpsid() const { return _fpsid; }

  static void copy_state(Fpu_state *to, Fpu_state const *from)
  {
    memcpy(to, from, state_size());
  }

  static void restore_state(Fpu_state const *s, bool);
  static void save_state(Fpu_state *s);
  static void init(Cpu_number cpu, bool resume);

  static Mword mvfr0()
  {
    Mword v;
    asm volatile(".fpu vfp\n vmrs %0, mvfr0" : "=r" (v));
    return v;
  }

  static Mword mvfr1()
  {
    Mword v;
    asm volatile(".fpu vfp\n vmrs %0, mvfr1" : "=r" (v));
    return v;
  }

  static void fpexc(Mword v)
  {
    asm volatile(".fpu vfp\n vmsr fpexc, %0" : : "r" (v));
  }

  static Mword fpexc()
  {
    Mword v;
    asm volatile(".fpu vfp\n vmrs %0, fpexc" : "=r" (v));
    return v;
  }

  static Mword fpinst()
  {
    Mword i;
    asm volatile(".fpu vfp\n vmrs %0, fpinst" : "=r" (i));
    return i;
  }

  static void fpinst(Mword v)
  {
    asm volatile(".fpu vfp\n vmsr fpinst, %0" : : "r" (v));
  }

  static Mword fpinst2()
  {
    Mword i;
    asm volatile(".fpu vfp\n vmrs %0, fpinst2" : "=r" (i));
    return i;
  }

  static void fpinst2(Mword v)
  {
    asm volatile(".fpu vfp\n vmsr fpinst2, %0" : : "r" (v));
  }

  static bool exc_pending()
  {
    return fpexc() & FPEXC_EX;
  }

  static Emulate_result emulate_insns(Mword opcode, Trap_state *ts);

  static unsigned state_size()
  { return sizeof (Fpu_regs); }

  static unsigned state_align()
  { return 4; }

  static void
  save_user_exception_state(bool owner, Fpu_state_ptr const &s,
                            Trap_state *ts, Exception_state_user *esu);

#ifdef CONFIG_CPU_VIRT
private:
  Mword _fpexc;

public:
  static bool is_enabled()
  {
    Mword dummy; __asm__ __volatile__ ("mrc p15, 4, %0, c1, c1, 2" : "=r"(dummy));
    return !(dummy & 0xc00);
  }

  void enable()
  {
    Mword dummy;
    __asm__ __volatile__ (
        "mrc p15, 4, %0, c1, c1, 2 \n"
        "bic %0, %0, #0xc00        \n"
        "mcr p15, 4, %0, c1, c1, 2 \n" : "=&r" (dummy) );
    fpexc(_fpexc);
    Mem::isb();
  }

  void disable()
  {
    Mword dummy;
    if (!is_enabled())
      {
        if (!(_fpexc & FPEXC_EN))
          {
            enable();
            fpexc(_fpexc | FPEXC_EN);
          }
      }
    else
      {
        _fpexc = fpexc();
        if (!(_fpexc & FPEXC_EN))
          fpexc(_fpexc | FPEXC_EN);
      }
    __asm__ __volatile__ (
        "mrc p15, 4, %0, c1, c1, 2 \n"
        "orr %0, %0, #0xc00        \n"
        "mcr p15, 4, %0, c1, c1, 2 \n" : "=&r" (dummy) );
    Mem::isb();
  }

#else // CONFIG_CPU_VIRT

  static bool is_enabled()
  {
    return fpexc() & FPEXC_EN;
  }


  static void enable()
  {
    fpexc((fpexc() | FPEXC_EN)); // & ~FPEXC_EX);
    Mem::isb();
  }

  static void disable()
  {
    fpexc(fpexc() & ~FPEXC_EN);
    Mem::isb();
  }
#endif // CONFIG_CPU_VIRT

private:
  Fpsid _fpsid;
  static bool save_32r;
};

