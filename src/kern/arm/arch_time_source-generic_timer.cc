
#include <arch_time_source-generic_timer.h>


#include <types.h>
#include <warn.h>
#include <kip.h>

#include <kip_asm.h>
#include <kip_clock_init.h>

Fix_point_multiplier Arch_time_source_generic_timer::_scaler_shift_ts_to_ns;
Fix_point_multiplier Arch_time_source_generic_timer::_scaler_shift_ts_to_us;
Fix_point_multiplier Arch_time_source_generic_timer::_scaler_shift_us_to_ts;


void
Arch_time_source_generic_timer::setup_scalers(Unsigned32 freq)
{
  _scaler_shift_ts_to_ns = Fix_point_multiplier::calc(freq, 1'000'000'000);
  _scaler_shift_ts_to_us = Fix_point_multiplier::calc(freq, 1'000'000);
  _scaler_shift_us_to_ts = Fix_point_multiplier::calc(1'000'000, freq);
}

void
Arch_time_source_generic_timer::init_system_clock()
{
  extern unsigned char const _kip_time_code[];
  kip_clock_deploy_code_blob(Kip::k(), _kip_time_code);
  Mword *b = reinterpret_cast<Mword *>(Kip::k()->clock_blob());
  b[0x70 / sizeof(Mword)] = _scaler_shift_ts_to_us.scaler;
  b[0x78 / sizeof(Mword)] = _scaler_shift_ts_to_us.shift;
  b[0xf0 / sizeof(Mword)] = _scaler_shift_ts_to_ns.scaler;
  b[0xf8 / sizeof(Mword)] = _scaler_shift_ts_to_ns.shift;
}

#ifdef CONFIG_BIT64
#if defined(CONFIG_CPU_VIRT)
        /* Generic_timer::T<Hyp>, Generic_timer::T<Secure_hyp>:
         * CNTKCTL_EL1: EL0VTEN=1, EL0PTEN=1.
         * CNTHCTL_EL2: EL0VCTEN=0, EL0PCTEN=1.
         * Kernel uses physical counter.
         * Access to physical counter from non-secure PL0/PL1 allowed. */
#define READ_TIMER_COUNTER "mrs     x0, CNTPCT_EL0"
#elif defined(CONFIG_ARM_EM_TZ)
        /* Generic_timer::T<Physical>:
         * CNTKCTL_EL1: EL0VTEN=1, EL0PTEN=1.
         * Kernel uses physical counter.
         * Access to physical counter from PL0 allowed. */
#define READ_TIMER_COUNTER "mrs     x0, CNTPCT_EL0"
#else
        /* Generic_timer::T<Virtual>:
         * CNTKCTL_EL1: EL0VTEN=1, EL0PTEN=0.
         * Kernel uses virtual counter.
         * Access to virtual counter from PL0 allowed. */
#define READ_TIMER_COUNTER "mrs     x0, CNTVCT_EL0"
#endif

asm (R"(
        .pushsection .text.ktcgt, "a"
_kip_time_code: )" KIP_CODE_HDR(1f, 2f, 0, 2f) R"(
1:
        )" READ_TIMER_COUNTER R"(
        ldr     x4, 1b + 0x70  /* scaler */
        ldr     x5, 1b + 0x78  /* shift */
        umulh   x1, x0, x4
        mul     x0, x0, x4     /* {x1,x0} contains (timer * scaler) */
        mov     x2, #32
        sub     x3, x2, x5
        add     x2, x2, x5
        lsr     x0, x0, x3     /* x0 = x0 >> (32 - shift) */
        lsl     x1, x1, x2     /* x1 = x1 << (32 + shift) */
        orr     x0, x0, x1     /* OR the "out-shifted" bits to r0 */
        ret
2: )" KIP_CODE_HDR(1f, 2f, 0x80, 2b) R"(
1: )" READ_TIMER_COUNTER R"(
        ldr     x4, 1b + 0xf0  /* scaler ns */
        ldr     x5, 1b + 0xf8  /* shift ns */
        umulh   x1, x0, x4
        mul     x0, x0, x4
        mov     x2, #32
        sub     x3, x2, x5
        add     x2, x2, x5
        lsr     x0, x0, x3
        lsl     x1, x1, x2
        orr     x0, x0, x1
        ret
2:
        .popsection
  )");

#endif // CONFIG_BIT64

#ifdef CONFIG_BIT32
#if defined(CONFIG_CPU_VIRT)
        /* Generic_timer::T<Hyp>, Generic_timer::T<Secure_hyp>:
         * CNTKCTL: PL0VCTEN=1, PL0PCTEN=1.
         * CNTHCTL: PL1PCEN=0, PL1PCTEN=1.
         * Kernel uses physical counter.
         * Access to physical counter from non-secure PL0/PL1 allowed. */
#define READ_TIMER_COUNTER "mrrc    p15, 0, r0, r1, c14"
#elif defined(CONFIG_ARM_EM_TZ)
        /* Generic_timer::T<Physical>:
         * CNTKCTL: PL0VCTEN=1, PL0PCTEN=1.
         * Kernel uses physical counter.
         * Access to physical counter from PL0 allowed. */
#define READ_TIMER_COUNTER "mrrc    p15, 0, r0, r1, c14"
#else
        /* Generic_timer::T<Virtual>:
         * CNTKCTL: PL0VCTEN=1, PL0PCTEN=0.
         * Kernel uses virtual counter.
         * Access to virtual counter from PL0 allowed. */
#define READ_TIMER_COUNTER "mrrc    p15, 1, r0, r1, c14"
#endif

asm (R"(
_kip_time_code: )" KIP_CODE_HDR(1f, 2f, 0, 2f) R"(
1: )" READ_TIMER_COUNTER R"(
        push    {r4, r5, r6, r7}
        ldr     r6, 1b + 0x70  /* scaler */
        ldr     r7, 1b + 0x78   /* shift */
        umull   r0, r3, r6, r0
        umull   r4, r5, r6, r1
        adds    r1, r4, r3
        adc     r2, r5, #0      /* {r2,r1,r0} contains (timer * scaler) */
        mov     r3, #32
        sub     r3, r3, r7
                                /* compute {r2,r1,r0} >> (32 - shift) */
        lsr     r0, r0, r3
        orr     r0, r0, r1, LSL r7 /* r0 = (r1<<shift) | (r0 >> (32-shift)) */
        lsr     r1, r1, r3
        orr     r1, r1, r2, LSL r7 /* r1 = (r2<<shift) | (r1 >> (32-shift)) */
        pop     {r4, r5, r6, r7}
        bx      lr
2: )" KIP_CODE_HDR(1f, 2f, 0x80, 2b) R"(
1: )" READ_TIMER_COUNTER R"(
        push    {r4, r5, r6, r7}
        ldr     r6, 1b + 0xf0
        ldr     r7, 1b + 0xf8
        umull   r0, r3, r6, r0
        umull   r4, r5, r6, r1
        adds    r1, r4, r3
        adc     r2, r5, #0
        mov     r3, #32
        sub     r3, r3, r7
        lsr     r0, r0, r3
        orr     r0, r0, r1, LSL r7
        lsr     r1, r1, r3
        orr     r1, r1, r2, LSL r7
        pop     {r4, r5, r6, r7}
        bx      lr
2: )");

#endif // CONFIG_BIT32

