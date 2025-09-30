#pragma once

#include <globalconfig.h>
#include <types.h>

namespace Generic_timer {

  enum Timer_type
  {
    Physical,   ///< EL1 physical timer
    Virtual,    ///< EL1 virtual timer
    Hyp,        ///< Non-secure EL2 physical timer
    Secure_hyp, ///< Secure EL2 physical timer
  };

  template<unsigned Type>  struct T;

  template<> struct T<Virtual>
  {
    static constexpr int Type = Virtual;

    /* In non-HYP mode we use always the virtual counter and the
     * virtual timer
     */
    static Unsigned64 counter() // use the virtual counter
    { Unsigned64 v; asm volatile("mrs %0, CNTVCT_EL0" : "=r" (v)); return v; }

    static Unsigned64 compare() // use virtual compare
    { Unsigned64 v; asm volatile("mrs %0, CNTV_CVAL_EL0" : "=r" (v)); return v; }

    static void compare(Unsigned64 v)
    { asm volatile("msr CNTV_CVAL_EL0, %0" : : "r" (v)); }

    static Unsigned32 control()
    { Mword v; asm volatile("mrs %0, CNTV_CTL_EL0" : "=r" (v)); return v; }

    static void control(Unsigned32 v)
    { asm volatile("msr CNTV_CTL_EL0, %x0" : : "r" (v)); }

    static void setup_timer_access()
    {
      // CNTKCTL: allow access to virtual counter from EL0
      asm volatile("msr CNTKCTL_EL1, %0" : : "r"(0x2UL));
    }

    static Unsigned32 frequency()
    { Mword v; asm volatile ("mrs %0, CNTFRQ_EL0": "=r" (v)); return v; }

    static void frequency(Unsigned32 v)
    { asm volatile ("msr CNTFRQ_EL0, %x0" : : "r" (v)); }
  };

  template<> struct T<Physical>
  {
    static constexpr int Type = Physical;

    /* In non-HYP mode we use always the virtual counter and the
     * virtual timer
     */
    static Unsigned64 counter() // use the physical counter
    { Unsigned64 v; asm volatile("mrs %0, CNTPCT_EL0" : "=r" (v)); return v; }

    static Unsigned64 compare() // use EL1 physical compare
    { Unsigned64 v; asm volatile("mrs %0, CNTP_CVAL_EL0" : "=r" (v)); return v; }

    static void compare(Unsigned64 v)
    { asm volatile("msr CNTP_CVAL_EL0, %0" : : "r" (v)); }

    static Unsigned32 control()
    { Mword v; asm volatile("mrs %0, CNTP_CTL_EL0" : "=r" (v)); return v; }

    static void control(Unsigned32 v)
    { asm volatile("msr CNTP_CTL_EL0, %x0" : : "r" (v)); }

    static void setup_timer_access()
    {
       // CNTKCTL: allow access to virtual and physical counter from EL0
      asm volatile("msr CNTKCTL_EL1, %0" : : "r"(0x3UL));
    }

    static Unsigned32 frequency()
    { Mword v; asm volatile ("mrs %0, CNTFRQ_EL0": "=r" (v)); return v; }

    static void frequency(Unsigned32 v)
    { asm volatile ("msr CNTFRQ_EL0, %x0" : : "r" (v)); }
  };

  template<> struct T<Hyp>
  {
    static constexpr int Type = Hyp;

    /* In HYP mode we use the physical counter and the
     * HYP mode timer
     */
    static Unsigned64 counter() // use the physical counter
    { Unsigned64 v; asm volatile("mrs %0, CNTPCT_EL0" : "=r" (v)); return v; }

    static Unsigned64 compare() // use non-secure EL2 physical compare
    { Unsigned64 v; asm volatile("mrs %0, CNTHP_CVAL_EL2" : "=r" (v)); return v; }

    static void compare(Unsigned64 v)
    { asm volatile("msr CNTHP_CVAL_EL2, %0" : : "r" (v)); }

    static Unsigned32 control()
    { Mword v; asm volatile("mrs %0, CNTHP_CTL_EL2" : "=r" (v)); return v; }

    static void control(Unsigned32 v)
    { asm volatile("msr CNTHP_CTL_EL2, %x0" : : "r" (v)); }

    static void setup_timer_access()
    {
      // CNTKCTL: allow access to virtual and physical counter from EL0
      asm volatile("msr CNTKCTL_EL1, %0" : : "r"(0x3UL));
      // CNTHCTL: forbid access to physical timer from EL0 and EL1
      asm volatile("msr CNTHCTL_EL2, %0" : : "r"(0x0UL));
      // CNTVOFF: sync virtual and physical counter
      asm volatile ("msr CNTVOFF_EL2, %x0" : : "r"(0));
    }

    static Unsigned32 frequency()
    { Mword v; asm volatile ("mrs %0, CNTFRQ_EL0": "=r" (v)); return v; }

    static void frequency(Unsigned32 v)
    { asm volatile ("msr CNTFRQ_EL0, %x0" : : "r" (v)); }
  };

  template<> struct T<Secure_hyp>
  {
    static constexpr int Type = Secure_hyp;

    /* In secure HYP mode we use the physical counter and the
     * secure HYP mode timer
     */
    static Unsigned64 counter() // use the physical counter
    { Unsigned64 v; asm volatile("mrs %0, CNTPCT_EL0" : "=r" (v)); return v; }

    static Unsigned64 compare() // use secure EL2 physical compare
    { Unsigned64 v; asm volatile("mrs %0, CNTHPS_CVAL_EL2" : "=r" (v)); return v; }

    static void compare(Unsigned64 v)
    { asm volatile("msr CNTHPS_CVAL_EL2, %0" : : "r" (v)); }

    static Unsigned32 control()
    { Mword v; asm volatile("mrs %0, CNTHPS_CTL_EL2" : "=r" (v)); return v; }

    static void control(Unsigned32 v)
    { asm volatile("msr CNTHPS_CTL_EL2, %x0" : : "r" (v)); }

    static void setup_timer_access()
    {
      // CNTKCTL: allow access to virtual and physical counter from EL0
      asm volatile("msr CNTKCTL_EL1, %0" : : "r"(0x3UL));
      // CNTHCTL: forbid access to physical timer from EL0 and EL1
      asm volatile("msr CNTHCTL_EL2, %0" : : "r"(0x0UL));
      // CNTVOFF: sync virtual and physical counter
      asm volatile ("msr CNTVOFF_EL2, %x0" : : "r"(0));
    }

    static Unsigned32 frequency()
    { Mword v; asm volatile ("mrs %0, CNTFRQ_EL0": "=r" (v)); return v; }

    static void frequency(Unsigned32 v)
    { asm volatile ("msr CNTFRQ_EL0, %x0" : : "r" (v)); }
  };

#ifdef CONFIG_CPU_VIRT
  using Gtimer = Generic_timer::T<Generic_timer::Hyp>;
#elif defined (CONFIG_ARM_EM_TZ)
  using Gtimer = Generic_timer::T<Generic_timer::Physical>;
#else
  using Gtimer = Generic_timer::T<Generic_timer::Virtual>;
#endif
}

