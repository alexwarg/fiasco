#pragma once
#if defined(__arm__) || defined(__thumb__)
inline uint64_t
Fix_point_multiplier::multiply(uint64_t v) const
{
  unsigned long lo = v & 0xffffffff;
  unsigned long hi = v >> 32;
  unsigned long dummy1, dummy2, dummy3, dummy4;
  // This code is written in Assembler so that it doesn't require libgcc. It
  // should be also very similar to kip_time_fn_read_us()/kip_time_fn_read_us()
  // used by userland to get the same results as the kernel code.
  asm ("umull   %0, %3, %[scaler], %0   \n\t"
       "umull   %4, %5, %[scaler], %1   \n\t"
       "adds    %1, %4, %3              \n\t"
       "adc     %2, %5, #0              \n\t"
       "mov     %3, #32                 \n\t"
       "sub     %3, %3, %[shift]        \n\t"
#ifdef __thumb__
       "lsl     %4, %1, %[shift]        \n\t"
       "lsl     %5, %2, %[shift]        \n\t"
       "lsr     %0, %0, %3              \n\t"
       "orr     %0, %0, %4              \n\t"
       "lsr     %1, %1, %3              \n\t"
       "orr     %1, %1, %5              \n\t"
#else
       "lsr     %0, %0, %3              \n\t"
       "orr     %0, %0, %1, LSL %[shift]\n\t"
       "lsr     %1, %1, %3              \n\t"
       "orr     %1, %1, %2, LSL %[shift]\n\t"
#endif
       : "+r"(lo), "+r"(hi),
         "=&r"(dummy1), "=&r"(dummy2), "=&r"(dummy3), "=&r"(dummy4)
       : [scaler]"r"(scaler), [shift]"r"(shift)
       : "cc");
  return ((uint64_t)hi << 32) | lo;
}
#elif defined(__aarch64__)
inline uint64_t
Fix_point_multiplier::multiply(uint64_t v) const
{
  unsigned long dummy1, dummy2, dummy3;
  // This code is written in Assembler so that it doesn't require libgcc. It
  // should be also very similar to kip_time_fn_read_us()/kip_time_fn_read_us()
  // used by userland to get the same results as the kernel code.
  asm ("umulh   %1, %0, %x[scaler]       \n\t"
       "mul     %0, %0, %x[scaler]       \n\t"
       "mov     %2, #32                 \n\t"
       "sub     %3, %2, %x[shift]        \n\t"
       "add     %2, %2, %x[shift]        \n\t"
       "lsr     %0, %0, %3              \n\t"
       "lsl     %1, %1, %2              \n\t"
       "orr     %0, %0, %1              \n\t"
       : "+r"(v), "=&r"(dummy1), "=&r"(dummy2), "=&r"(dummy3)
       : [scaler]"r"(scaler), [shift]"r"(shift)
       : "cc");
  return v;
}
#endif
