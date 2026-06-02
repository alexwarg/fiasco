
#include <arch_time_source.h>
#include <kip_asm.h>
#include <kip.h>
#include <kip_clock_init.h>
#include <globalconfig.h>

#define _STR(v)     __STR(v)
#define __STR(v)    #v

#ifdef CONFIG_BIT32
asm (
R"(
    .pushsection ".text.ktc", "ax"
    _kip_time_code: )"
    KIP_CODE_HDR(1f, 5f, 0, 8f) R"(
    1: push    %ebx
       call    2f
    2: pop     %ebx
       rdtsc
       movl    %edx, %ecx
       mull    (0xf0 - (2b - 1b))(%ebx)
       movl    %ecx, %eax
       movl    %edx, %ecx
       mull    (0xf0 - (2b - 1b))(%ebx)
       addl    %ecx, %eax
       adcl    $0, %edx
       pop     %ebx
       ret
    5:

    8:)" KIP_CODE_HDR(1f, 5f, 0x80, 8b) R"(
    1: push    %ebx
       call    2f
    2: pop     %ebx
       rdtsc
       push    %ebp
       movl    %edx, %ecx      # eax = tsc.lo, edx = tsc.hi
       mull    (0xf8 - (2b - 1b))(%ebx)
       movl    %eax, %ebp      # ebp = (tsc.lo * scaler).lo
       movl    %ecx, %eax      # eax = tsc.hi
       movl    %edx, %ecx      # ecx = (tsc.lo * scaler).hi
       mull    (0xf8 - (2b - 1b))(%ebx)
       addl    %ecx, %eax
       adcl    $0, %edx        # edx:eax:ebp = tsc * scaler
       shld    $5, %eax, %edx
       shld    $5, %ebp, %eax
       pop     %ebp            # edx:eax = (tsc * scaler) >> (32 - 5)
       pop     %ebx
       ret
    5:
    .popsection
)"
    );
#endif


#ifdef CONFIG_BIT64
asm (R"(
    .pushsection ".text.ktc", "ax"
    _kip_time_code: )"
    KIP_CODE_HDR(1f, 5f, 0, 8f) R"(
    1: rdtsc
       shl     $32, %rdx
       or      %rdx, %rax
       mulq    (0xf0 + 1b)(%rip)  # scaler is at 1b + 0xf0 == 0x9f0
       shrd    $32, %rdx, %rax
       ret
    5:

    8: )"
    KIP_CODE_HDR(1f, 5f, 0x80, 8b) R"(
    1: rdtsc
       shl     $32, %rdx
       or      %rdx, %rax
       mulq    (0xf8 +1b)(%rip)    # scaler is at 1b+0xf8 == 0x9f8
       shrd    $32, %rdx, %rax
       ret
    5:
    .popsection
)");
#endif

void
Arch_time_source::init_system_clock()
{
  extern unsigned char const _kip_time_code[];
  kip_clock_deploy_code_blob(Kip::k(), _kip_time_code);
  Cpu &cpu = Cpu::cpus.cpu(Cpu_number::boot_cpu());
  *reinterpret_cast<Mword*>(Kip::k()->clock_blob() + 0xf0) = cpu.get_scaler_tsc_to_us();
  *reinterpret_cast<Mword*>(Kip::k()->clock_blob() + 0xf8) = cpu.get_scaler_tsc_to_ns();
}

