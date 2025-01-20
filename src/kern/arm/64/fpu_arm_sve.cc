
#include <fpu.h>
#include <ram_quota.h>
#include <kmem_slab.h>
#include <panic.h>
#include <cxx/type_traits>
#include <globalconfig.h>
#include <fpu_state_arm_simd.h>

static bool _has_sve;
static unsigned _max_vl;

#if ! defined (CONFIG_CPU_VIRT)
static inline void enable_sve()
{
  Mword t;
  asm volatile("mrs  %0, CPACR_EL1  \n"
               "orr  %0, %0, %1     \n"
               "msr  CPACR_EL1, %0  \n"
               : "=r"(t) : "I" (Cpu::Cpacr_el1_zen_full));
  Mem::isb();
}

static inline void disable_sve()
{
  Mword t;
  asm volatile("mrs  %0, CPACR_EL1  \n"
               "bic  %0, %0, %1     \n"
               "msr  CPACR_EL1, %0  \n"
               : "=r"(t) : "I" (Cpu::Cpacr_el1_zen_full));
  // No need for an ISB here, as returning to user mode already acts as
  // a context synchronization event.
}

#else

static inline void enable_sve()
{
  Mword dummy;
  __asm__ __volatile__ (
      "mrs %0, CPTR_EL2         \n"
      "bic %0, %0, %1           \n"
      "msr CPTR_EL2, %0         \n"
      : "=&r" (dummy) : "I" (Cpu::Cptr_el2_tz));
  Mem::isb();
}

static inline void disable_sve()
{
  Mword dummy;
  __asm__ __volatile__ (
      "mrs  %0, CPTR_EL2           \n"
      "orr  %0, %0, %1             \n"
      "msr  CPTR_EL2, %0           \n"
      : "=&r" (dummy) : "I" (Cpu::Cptr_el2_tz));
  // No need for an ISB here, as returning to user mode already acts as
  // a context synchronization event.
}

#endif



class Fpu_state_sve : public Fpu_state
{
public:
  Fpu_state_type type() const override
  { return Fpu_state_type::Sve; }

  enum
  {
    Num_z_regs = 32,
    Num_p_regs = 16,
  };

  template<typename T, unsigned SIZE_CONST, unsigned SIZE_VL, typename PREV,
           unsigned ALIGN = alignof(T)>
  struct Element
  {
    using Type = T;

    static constexpr unsigned off(unsigned vl)
    {
      if constexpr (cxx::is_same_v<PREV, void>)
        return 0;
      else
        return PREV::off(vl) + PREV::size(vl);
    }

    static constexpr unsigned size(unsigned vl)
    { return SIZE_CONST + vl * SIZE_VL; }
#if 0
    static_assert(off(1) % ALIGN == 0,
                  "Broken alignment for odd vector lengths.");
    static_assert(off(2) % ALIGN == 0,
                  "Broken alignment for even vector lengths.");
#endif
  };

  /// Vector registers Z0..Z31 (must be 16-byte aligned)
  using Z = Element<Unsigned64, 0, Num_z_regs * 16, void, 16>;
  /// SVE control register (holds vector length selected by EL1 userspace)
  using Zcr = Element<Unsigned32, sizeof(Unsigned32), 0, Z>;
  using Fpcr = Element<Unsigned32, sizeof(Unsigned32), 0, Zcr>;
  using Fpsr = Element<Unsigned32, sizeof(Unsigned32), 0, Fpcr>;
  /// Predicate registers P0..P15
  using P = Element<Unsigned16, 0, Num_p_regs * 2, Fpsr>;
  /// First-fault register (same length and layout as a P register)
  using Ffr = Element<Unsigned16, 0, 2, P>;

  static unsigned dyn_size()
  {
    using End = Element<Unsigned8, 0, 0, Ffr>;
    return End::off(_max_vl);
  }

  alignas(16) Unsigned8 ext_state[0];

  template<typename E>
  inline typename E::Type *access()
  {
    return reinterpret_cast<typename E::Type *>(ext_state + E::off(_max_vl));
  }

  template<typename E>
  inline typename E::Type const *get() const
  {
    return reinterpret_cast<typename E::Type const *>(ext_state + E::off(_max_vl));
  }

  void operator delete (void *addr, size_t s);

  Fpu_state_sve()
  {
    memset(ext_state, 0, dyn_size());

    // Default to maximum vector length
    *access<Zcr>() = _max_vl - 1;
  }

  void init_from_simd()
  {
    auto zcr = Cpu::zcr();
    // Drop ZCR temporarily to the size of FP/SIMD registers (128-bit)
    Cpu::zcr(Cpu::Zcr_vl_128);
    save_regs();
    // Restore previous ZCR
    Cpu::zcr(zcr);
  }

  void copy(Fpu_state const *from) override
  {
    memcpy(ext_state, nonull_static_cast<Fpu_state_sve const *>(from)->ext_state,
           Fpu_state_sve::dyn_size());
  }


protected:
  void save_regs()
  {
    Mword fpcr;
    Mword fpsr;
    asm volatile(// Vector registers
                 ".arch_extension sve             \n"
                 "str z0,  [%[z], #0, mul vl]     \n"
                 "str z1,  [%[z], #1, mul vl]     \n"
                 "str z2,  [%[z], #2, mul vl]     \n"
                 "str z3,  [%[z], #3, mul vl]     \n"
                 "str z4,  [%[z], #4, mul vl]     \n"
                 "str z5,  [%[z], #5, mul vl]     \n"
                 "str z6,  [%[z], #6, mul vl]     \n"
                 "str z7,  [%[z], #7, mul vl]     \n"
                 "str z8,  [%[z], #8, mul vl]     \n"
                 "str z9,  [%[z], #9, mul vl]     \n"
                 "str z10, [%[z], #10, mul vl]    \n"
                 "str z11, [%[z], #11, mul vl]    \n"
                 "str z12, [%[z], #12, mul vl]    \n"
                 "str z13, [%[z], #13, mul vl]    \n"
                 "str z14, [%[z], #14, mul vl]    \n"
                 "str z15, [%[z], #15, mul vl]    \n"
                 "str z16, [%[z], #16, mul vl]    \n"
                 "str z17, [%[z], #17, mul vl]    \n"
                 "str z18, [%[z], #18, mul vl]    \n"
                 "str z19, [%[z], #19, mul vl]    \n"
                 "str z20, [%[z], #20, mul vl]    \n"
                 "str z21, [%[z], #21, mul vl]    \n"
                 "str z22, [%[z], #22, mul vl]    \n"
                 "str z23, [%[z], #23, mul vl]    \n"
                 "str z24, [%[z], #24, mul vl]    \n"
                 "str z25, [%[z], #25, mul vl]    \n"
                 "str z26, [%[z], #26, mul vl]    \n"
                 "str z27, [%[z], #27, mul vl]    \n"
                 "str z28, [%[z], #28, mul vl]    \n"
                 "str z29, [%[z], #29, mul vl]    \n"
                 "str z30, [%[z], #30, mul vl]    \n"
                 "str z31, [%[z], #31, mul vl]    \n"

                 // Predicate registers
                 "str p0,  [%[p], #0, mul vl]     \n"
                 "str p1,  [%[p], #1, mul vl]     \n"
                 "str p2,  [%[p], #2, mul vl]     \n"
                 "str p3,  [%[p], #3, mul vl]     \n"
                 "str p4,  [%[p], #4, mul vl]     \n"
                 "str p5,  [%[p], #5, mul vl]     \n"
                 "str p6,  [%[p], #6, mul vl]     \n"
                 "str p7,  [%[p], #7, mul vl]     \n"
                 "str p8,  [%[p], #8, mul vl]     \n"
                 "str p9,  [%[p], #9, mul vl]     \n"
                 "str p10, [%[p], #10, mul vl]    \n"
                 "str p11, [%[p], #11, mul vl]    \n"
                 "str p12, [%[p], #12, mul vl]    \n"
                 "str p13, [%[p], #13, mul vl]    \n"
                 "str p14, [%[p], #14, mul vl]    \n"
                 "str p15, [%[p], #15, mul vl]    \n"

                 // FFR
                 "rdffr p0.b                      \n"
                 "str p0,  [%[ffr], #0, mul vl]   \n"
                 "ldr p0,  [%[p], #0, mul vl]     \n"

                 "mrs     %[fpcr], fpcr           \n"
                 "mrs     %[fpsr], fpsr           \n"
                 ".arch_extension nosve           \n"
                 : [fpcr] "=r" (fpcr),
                   [fpsr] "=r" (fpsr)
                 : [z] "r" (access<Z>()),
                   [p] "r" (access<P>()),
                   [ffr] "r" (access<Ffr>()));

    *access<Fpcr>() = fpcr;
    *access<Fpsr>() = fpsr;
  }

  void restore_regs() const
  {
    asm volatile(// Vector registers
                 ".arch_extension sve             \n"
                 "ldr z0,  [%[z], #0, mul vl]     \n"
                 "ldr z1,  [%[z], #1, mul vl]     \n"
                 "ldr z2,  [%[z], #2, mul vl]     \n"
                 "ldr z3,  [%[z], #3, mul vl]     \n"
                 "ldr z4,  [%[z], #4, mul vl]     \n"
                 "ldr z5,  [%[z], #5, mul vl]     \n"
                 "ldr z6,  [%[z], #6, mul vl]     \n"
                 "ldr z7,  [%[z], #7, mul vl]     \n"
                 "ldr z8,  [%[z], #8, mul vl]     \n"
                 "ldr z9,  [%[z], #9, mul vl]     \n"
                 "ldr z10, [%[z], #10, mul vl]    \n"
                 "ldr z11, [%[z], #11, mul vl]    \n"
                 "ldr z12, [%[z], #12, mul vl]    \n"
                 "ldr z13, [%[z], #13, mul vl]    \n"
                 "ldr z14, [%[z], #14, mul vl]    \n"
                 "ldr z15, [%[z], #15, mul vl]    \n"
                 "ldr z16, [%[z], #16, mul vl]    \n"
                 "ldr z17, [%[z], #17, mul vl]    \n"
                 "ldr z18, [%[z], #18, mul vl]    \n"
                 "ldr z19, [%[z], #19, mul vl]    \n"
                 "ldr z20, [%[z], #20, mul vl]    \n"
                 "ldr z21, [%[z], #21, mul vl]    \n"
                 "ldr z22, [%[z], #22, mul vl]    \n"
                 "ldr z23, [%[z], #23, mul vl]    \n"
                 "ldr z24, [%[z], #24, mul vl]    \n"
                 "ldr z25, [%[z], #25, mul vl]    \n"
                 "ldr z26, [%[z], #26, mul vl]    \n"
                 "ldr z27, [%[z], #27, mul vl]    \n"
                 "ldr z28, [%[z], #28, mul vl]    \n"
                 "ldr z29, [%[z], #29, mul vl]    \n"
                 "ldr z30, [%[z], #30, mul vl]    \n"
                 "ldr z31, [%[z], #31, mul vl]    \n"

                 // FFR
                 "ldr p0,  [%[ffr], #0, mul vl]   \n"
                 "wrffr p0.b                      \n"

                 // Predicate registers
                 "ldr p0,  [%[p], #0, mul vl]     \n"
                 "ldr p1,  [%[p], #1, mul vl]     \n"
                 "ldr p2,  [%[p], #2, mul vl]     \n"
                 "ldr p3,  [%[p], #3, mul vl]     \n"
                 "ldr p4,  [%[p], #4, mul vl]     \n"
                 "ldr p5,  [%[p], #5, mul vl]     \n"
                 "ldr p6,  [%[p], #6, mul vl]     \n"
                 "ldr p7,  [%[p], #7, mul vl]     \n"
                 "ldr p8,  [%[p], #8, mul vl]     \n"
                 "ldr p9,  [%[p], #9, mul vl]     \n"
                 "ldr p10, [%[p], #10, mul vl]    \n"
                 "ldr p11, [%[p], #11, mul vl]    \n"
                 "ldr p12, [%[p], #12, mul vl]    \n"
                 "ldr p13, [%[p], #13, mul vl]    \n"
                 "ldr p14, [%[p], #14, mul vl]    \n"
                 "ldr p15, [%[p], #15, mul vl]    \n"

                 "msr     fpcr, %[fpcr]           \n"
                 "msr     fpsr, %[fpsr]           \n"
                 ".arch_extension nosve           \n"
                 : : [z] "r" (get<Z>()),
                     [p] "r" (get<P>()),
                     [ffr] "r" (get<Ffr>()),
                     [fpcr] "r" (get<Fpcr>()),
                     [fpsr] "r" (get<Fpsr>()));
  }

  void save() override;
  void restore() const override;
};


#if ! defined (CONFIG_CPU_VIRT)

void
Fpu_state_sve::save()
{ save_regs(); }

void
Fpu_state_sve::restore() const
{
  enable_sve();
  restore_regs();
}

#else

template<typename CB> inline
void with_adjusted_vl(Mword sel_zcr, CB &&cb)
{
  // Vector length selected by user mode
  unsigned sel_zcr_vl = (sel_zcr & Cpu::Zcr_vl_mask);
  // Maximum vector length supported by the CPU and Fiasco
  unsigned max_zcr_vl = _max_vl - 1;
  // Avoid operating on unused bytes in the SVE registers if user mode uses a
  // smaller vector length than the maximum supported by Fiasco!
  bool drop_vl = sel_zcr_vl < max_zcr_vl;
  if (drop_vl)
    Cpu::zcr(sel_zcr_vl);

  cb();

  // Restore maximum vector length
  if (drop_vl)
    Cpu::zcr(max_zcr_vl);
}

void
Fpu_state_sve::save()
{
  // Save vector length selected by user mode
  Mword zcr_el1 = Cpu::zcr_el1();
  *access<Zcr>() = zcr_el1;

  with_adjusted_vl(zcr_el1, [=]() { save_regs(); });
}

void
Fpu_state_sve::restore() const
{
  enable_sve();
  Mword zcr_el1 = *get<Zcr>();

  with_adjusted_vl(zcr_el1, [=]() { restore_regs(); });

  // Restore vector length selected by user mode
  Cpu::zcr_el1(zcr_el1);
}
#endif

class Fpu_state_simd_x : public Fpu_state, Fpu_state_simd
{
public:
  Fpu_state_type type() const override
  { return Fpu_state_type::Simd; }

  void save() override
  { Fpu_state_simd::save(); }

  void restore() const override
  {
    disable_sve();
    Fpu_state_simd::restore();
  }

  void copy(Fpu_state const *from) override
  { Fpu_state_simd::copy(nonull_static_cast<Fpu_state_simd_x const *>(from)); }

  void operator delete (void *addr, size_t s);
};


static void detect_sve(Cpu_number cpu)
{
  bool has_sve = Cpu::cpus.cpu(cpu).has_sve();
  unsigned max_vl = 0;
  if (has_sve)
    {
      // Enable both SVE and FP/SIMD, because if we only enable SVE, the FP/SIMD
      // trap mechanism will trigger when we access the ZCR register or execute
      // the rdvl instruction.
      enable_sve();
      bool fpsimd_enabled = Fpu::is_enabled();
      if (!fpsimd_enabled)
        Fpu::enable();

      // Detect maximum supported VL
      Cpu::zcr(Cpu::Zcr_vl_max);
      max_vl = Cpu::sve_vl();

      // At least 128-bit, the FP/SIMD registers, must be present
      if (max_vl < 1)
        panic("Minimum required SVE vector length not available!");

      Cpu::zcr(max_vl - 1);

      disable_sve();
      if (!fpsimd_enabled)
        Fpu::disable();
    }

  if (cpu == Cpu_number::boot_cpu())
    {
      if (has_sve)
        printf("CPU implements SVE with a vector length of up to %u bits.\n",
               max_vl * 128);

      _has_sve = has_sve;
      _max_vl = max_vl;
    }
  else
    {
      // We assume that all CPUs exhibit the same SVE limits.
      assert(_has_sve == has_sve);
      assert(_max_vl == max_vl);
    }
}

static void init_sve_from_simd(Fpu_state *fpu_state)
{
  assert(fpu_state->type() == Fpu::State_type::Sve);
  enable_sve();
  nonull_static_cast<Fpu_state_sve *>(fpu_state)->init_from_simd();
}

unsigned
Fpu_arch_base::state_size(State_type type)
{
  if (type == State_type::Sve)
    return sizeof(Fpu_state_sve) + Fpu_state_sve::dyn_size();
  else
    return sizeof(Fpu_state_simd_x);
}

constexpr unsigned
quota_offset(unsigned state_size)
{
  return (state_size + alignof(Ram_quota *) - 1) & ~(alignof(Ram_quota *) - 1);
}

constexpr unsigned
size_with_quota(unsigned state_size)
{
  return quota_offset(state_size) + sizeof(Ram_quota *);
}

static Kmem_slab _sve_state_allocator(
    size_with_quota(Fpu::state_size(Fpu::State_type::Sve)),
    Fpu::state_align(), "Sve state");

static Kmem_slab _fpu_state_allocator(
    size_with_quota(Fpu::state_size(Fpu::Default_state_type)),
    Fpu::state_align(), "Fpu state");


template<typename T>
inline Fpu_state *ctor_if(Ram_quota *q, void *m, unsigned sz)
{
  if (!m)
    return nullptr;

  Fpu_state *r = new (m) T();
  *offset_cast<Ram_quota **>(m, quota_offset(sz)) = q;
  return r;
}

void Fpu_state_sve::operator delete (void *sb, size_t)
{
  if (!sb)
    return;

  Ram_quota *q = *cxx::launder(offset_cast<Ram_quota **>(sb,
      quota_offset(Fpu::state_size(Fpu::State_type::Sve))));
  _sve_state_allocator.q_free(q, sb);
}

void Fpu_state_simd_x::operator delete (void *sb, size_t)
{
  if (!sb)
    return;

  Ram_quota *q = *cxx::launder(offset_cast<Ram_quota **>(sb,
      quota_offset(Fpu::state_size(Fpu::State_type::Simd))));
  _fpu_state_allocator.q_free(q, sb);
}

void
Fpu_arch_base::init(Cpu_number cpu, bool resume)
{
  if (!resume)
    detect_sve(cpu);
}

namespace Fpu_alloc
{

Fpu_state *alloc_types_state(Ram_quota *q, Fpu::State_type type)
{
  switch (type)
    {
    case Fpu::State_type::Sve:
      return ctor_if<Fpu_state_sve>(q, _sve_state_allocator.q_alloc(q), Fpu::state_size(type));
    case Fpu::State_type::Simd:
      return ctor_if<Fpu_state_simd_x>(q, _fpu_state_allocator.q_alloc(q), Fpu::state_size(type));
    default:
      return nullptr;
    }
}

}
