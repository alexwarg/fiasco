#pragma once

#include <cxx/bitfield>
#include <fpu_state.h>
#include <cp0_status.h>
#include <globalconfig.h>
#include <std_macros.h>
#include <mem.h>

#include <cassert>

class Fpu_arch
{
public:
  struct Fpu_regs
  {
    Unsigned64 regs[32];
    Mword fcsr;
  };

  struct Fir
  {
    Mword v;

    Fir() = default;
    explicit Fir(Mword v) : v(v) {}

    CXX_BITFIELD_MEMBER(0, 7, revision, v);
    CXX_BITFIELD_MEMBER(8, 15, processor_id, v);
    CXX_BITFIELD_MEMBER(16, 16, s, v);
    CXX_BITFIELD_MEMBER(17, 17, d, v);
    CXX_BITFIELD_MEMBER(18, 18, ps, v);
    //CXX_BITFIELD_MEMBER(19, 19, _3D, v);
    CXX_BITFIELD_MEMBER(20, 20, w, v);
    CXX_BITFIELD_MEMBER(21, 21, l, v);
    CXX_BITFIELD_MEMBER(22, 22, f64, v);
    //CXX_BITFIELD_MEMBER(23, 23, Has2008, v);
    //CXX_BITFIELD_MEMBER(24, 24, FC, v);
    CXX_BITFIELD_MEMBER(28, 28, ufrp, v);
    CXX_BITFIELD_MEMBER(29, 29, frep, v);
  };

  static Mword fir_read()
  {
    Mword fir;
    __asm__ __volatile__(
        ".set push    \n"
        ".set reorder \n"
        ".set mips1   \n"
        "cfc1 %0, $0  \n"
        ".set pop     \n"
        : "=r" (fir));
    return fir;
  }

  static Mword fcr_read()
  {
    Mword fcr;
    __asm__ __volatile__(
        ".set push    \n"
        ".set reorder \n"
        ".set mips1   \n"
        "cfc1 %0, $31 \n"
        ".set pop     \n"
        : "=r" (fcr));
    return fcr;
  }

  static Mword fcr(Fpu_state *s)
  {
    Fpu_regs *fpu_regs = reinterpret_cast<Fpu_regs *>(s->state_buffer());

    assert(fpu_regs);
    return fpu_regs->fcsr;
  }

  static bool mode_64bit()
  {
    if (IS_ENABLED(CONFIG_CPU_MIPSR6))
      return true;
    else
      return Cp0_status::read() & Cp0_status::ST_FR;
  }

  static unsigned state_size()
  { return sizeof (Fpu_regs); }

  static unsigned state_align()
  { return sizeof(Unsigned64); }

  static void init_state(Fpu_state *s)
  {
    Fpu_regs *fpu_regs = reinterpret_cast<Fpu_regs *>(s->state_buffer());
    static_assert(!(sizeof (*fpu_regs) % sizeof(Mword)),
                  "Non-mword size of Fpu_regs");

    // Load the FPU with signalling NANS.  This bit pattern we're using has
    // the property that no matter whether considered as single or as double
    // precision represents signaling NANS.
    Mem::memset_mwords(fpu_regs, -1UL, sizeof (*fpu_regs) / sizeof(Mword));

    // We initialize fcr31 to rounding to nearest, no exceptions.
    fpu_regs->fcsr = 0;
  }

  static bool is_enabled()
  {
    return Cp0_status::read() & Cp0_status::ST_CU1;
  }

  static void enable()
  {
    Cp0_status::set_status_bit(Cp0_status::ST_CU1);
  }

  static void disable()
  {
    Cp0_status::clear_status_bit(Cp0_status::ST_CU1);
  }

  static void init(Cpu_number cpu, bool resume);
  static void save_state(Fpu_state *s);
  static void restore_state(Fpu_state *s, bool owner);

  Fir fir() const { return _fir; }

private:
  Fir _fir;

  static void show(Cpu_number cpu);
};

