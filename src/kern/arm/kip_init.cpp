INTERFACE [arm]:

#include "kip.h"

class Kip_init
{
public:
  static void init();
};


//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include <cstring>

#include "config.h"
#include "mem_layout.h"
#include "mem_unit.h"

#include <kip_asm.h>
#include <kip_clock_init.h>
#include <globalconfig.h>


// Make the stuff below appearing only in this compilation unit.
// Trick Preprocess to let the struct reside in the cc file rather
// than putting it into the _i.h file which is perfectly wrong in 
// this case.
namespace KIP_namespace
{
  // See also restrictions about KIP layout in the kernel linker script!
  enum
  {
    Num_mem_descs = 64,
    Max_len_version = 512,

    Size_mem_descs = sizeof(Mword) * 2 * Num_mem_descs,
  };

  struct KIP
  {
    Kip kip;
    char mem_descs[Size_mem_descs];
  };

  KIP my_kernel_info_page asm("my_kernel_info_page") __attribute__((section(".kernel_info_page"))) =
    {
      {
	/* 00/00  */ L4_KERNEL_INFO_MAGIC,
	             Config::Kernel_version_id,
	             (Size_mem_descs + sizeof(Kip)) >> 4,
	             {}, 0, {},
	/* 10/20  */ 0, {},
	/* 20/40  */ 0, 0, {},
	/* 30/60  */ 0, 0, {},
	/* 40/80  */ 0, 0, {},
	/* 50/A0  */ 0, (sizeof(Kip) << (sizeof(Mword)*4)) | Num_mem_descs, {},
	/* 60/C0  */ {},
	/* A0/140 */ 0, 0, 0, 0,
	/* B8/160 */ {},
	/* E0/1C0 */ 0, 0, {},
	/* F0/1D0 */ {"", 0, {0}},
      },
      {}
    };
};

IMPLEMENT
void Kip_init::init()
{
  // Don't reference KIP::my_kernel_info_page directly because the actual
  // object contains more data: The linker script adds version information and
  // also extends the size to 4KiB. Using KIP::my_kernel_info_page directly
  // worries the compiler.
  extern char my_kernel_info_page[];
  Kip *kinfo = reinterpret_cast<Kip*>(my_kernel_info_page);
  Kip::init_global_kip(kinfo);
  kinfo->add_mem_region(Mem_desc(0, Mem_layout::User_max,
                        Mem_desc::Conventional, true));
  init_syscalls(kinfo);
}

#ifdef CONFIG_BIG_ENDIAN
#error "Adapt read_64bit to big endian!"
#endif

#ifdef CONFIG_BIT32
#if defined (__clang__)
asm (R"(
.macro adrl reg:req, label:req
        sub \reg, pc, #((-(\label - 4711\@f) - 0x800))
        sub \reg, \reg, #0x800
        4711\@:
.endm
)");
#endif
asm (
    ".macro read_barrier\n\t"
#if defined(CONFIG_ARM_V6)
    "mcr     p15, 0, r3, c7, c10, 5\n\t"
#elif defined(CONFIG_ARM_V7)
    "dmb     ish\n\t"
#elif defined(CONFIG_ARM_V8PLUS)
    "dmb     ishld\n\t"
#endif
    ".endm\n\t"
    );

/* Reads 64-bit value into r1:r0. Scrambles r2. */
#if (defined(CONFIG_ARM_V7) && defined(CONFIG_ARM_LPAE)) || defined(CONFIG_ARM_V8PLUS)
asm (R"(
.macro read_64bit mem
        adrl    r2, \mem
        ldrd    r0, [r2]
.endm )");
#else
asm (R"(
.macro read_64bit mem
1:      ldr     r1, \mem + 4
        read_barrier
        ldr     r0, \mem
        read_barrier
        ldr     r2, \mem + 4
        cmp     r1, r2
        bne     1b
.endm )");
#endif

/**
 * Provide the KIP clock value with a fine-grained resolution + accuracy.
 *
 * In case of CONFIG_ARM_SYNC_CLOCK is disabled, just provide the normal KIP
 * clock. Otherwise, read the ARM generic timer counter and transform it into
 * microseconds like done for the KIP clock value.
 * This code will be copied to the KIP at OFFS__KIP_FN_READ_US.
 *
 * The following formula is used to translate an ARM generic timer value into
 * a time value with microseconds resolution:
 *
 *                       timer value * scaler
 *   time(us, 64-bit) = ---------------------- * 2^shift
 *                               2^32
 */
asm (R"(
        .pushsection ".initcall.text", "ax"

_kip_time_code: )" KIP_CODE_HDR(1f, 2f, 0, 2f) R"(
1:      read_64bit 1b + 0xA0 - 0x900 /* 1b + KIP_CLOCK - clock_get_us_offset */
        bx      lr
2: )" KIP_CODE_HDR(1f, 2f, 0x80, 2b) R"(
1:      read_64bit 1b + 0xA0 - 0x980 /* 1b + KIP_CLOCK - clock_get_ns_offset */
        mov     r3, #1000
        umull   r0, r2, r3, r0  /* {r2,r0} = r3 * r0 */
        umull   r1, r12, r3, r1 /* {r12,r1} = r3 * r1 */
        adds    r1, r1, r2
        /* Just ignore the upper few bits in r12 and return {r1,r0}. */
        bx      lr
2:      .popsection )");
#endif // CONFIG_BIT32

#ifdef CONFIG_BIT64
/**
 * Provide the KIP clock value with a fine-grained resolution + accuracy.
 *
 * In case of CONFIG_ARM_SYNC_CLOCK is disabled, just provide the normal KIP
 * clock. Otherwise, read the ARM generic timer counter and transform it into
 * microseconds like done for the KIP clock value.
 * This code will be copied to the KIP at OFFS__KIP_FN_READ_US.
 *
 * The following formula is used to translate an ARM generic timer value into
 * a time value with microseconds resolution:
 *
 *                       timer value * scaler
 *   time(us, 64-bit) = ---------------------- * 2^shift
 *                               2^32
 */
asm (R"(
        .pushsection ".initcall.text", "ax"

_kip_time_code: )" KIP_CODE_HDR(1f, 2f, 0, 2f) R"(
1:      ldr     x0, 1b + 0x140 - 0x900 /* 1b + KIP_CLOCK - clock_get_us_offset */
        ret
2: )" KIP_CODE_HDR(1f, 2f, 0x80, 2b) R"(
1:      ldr     x0, 1b + 0x140 - 0x980 /* 1b + KIP_CLOCK - clock_get_us_offset */
        mov     x1, #1000
        mul     x0, x0, x1
        ret
2:      .popsection )");

#endif // CONFIG_BIT64

PUBLIC static
void
Kip_init::init_kip_clock()
{
  extern unsigned char const _kip_time_code[];
  kip_clock_deploy_code_blob(Kip::k(), _kip_time_code);
}

//--------------------------------------------------------------
IMPLEMENTATION[64bit && !cpu_virt]:

PRIVATE static inline
void
Kip_init::init_syscalls(Kip *kinfo)
{
  union K
  {
    Kip k;
    Mword w[0x1000 / sizeof(Mword)];
  };
  K *k = reinterpret_cast<K *>(kinfo);
  k->w[0x800 / sizeof(Mword)] = 0xd65f03c0d4000001; // svc #0; ret
}

//--------------------------------------------------------------
IMPLEMENTATION[64bit && cpu_virt]:

PRIVATE static inline
void
Kip_init::init_syscalls(Kip *kinfo)
{
  union K
  {
    Kip k;
    Mword w[0x1000 / sizeof(Mword)];
  };
  K *k = reinterpret_cast<K *>(kinfo);
  k->w[0x800 / sizeof(Mword)] = 0xd65f03c0d4000002; // hvc #0; ret
}

//--------------------------------------------------------------
IMPLEMENTATION[32bit]:

PRIVATE static inline
void
Kip_init::init_syscalls(Kip *)
{}
