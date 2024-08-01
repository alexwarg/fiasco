#include <kip_init.h>

#include <cstring>
#include <config.h>
#include <cpu.h>
#include <div32.h>
#include <kmem.h>
#include <panic.h>

#include <kip_asm.h>
#include <kip_clock_init.h>


/** KIP initialization. */
FIASCO_INIT
void
Kip_init::init_freq(Cpu const &cpu)
{
  Kip::k()->frequency_cpu	= div32(cpu.frequency(), 1000);
}


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
	/* F0/1E0 */ {"", 0, {}},
      },
      {}
    };
};

#ifdef CONFIG_AMD64

inline void reserve_amd64_hole()
{
  enum { Trigger = 0x0000800000000000UL };
  Kip::k()->add_mem_region(Mem_desc(Trigger, ~Trigger, 
	                   Mem_desc::Reserved, true));
}

#endif

#ifdef CONFIG_IA32
inline void reserve_amd64_hole()
{}
#endif

inline void setup_user_virtual(Kip *kinfo)
{
  kinfo->add_mem_region(Mem_desc(0, Mem_layout::User_max,
                        Mem_desc::Conventional, true));
}

extern char _boot_sys_start[];
extern char _boot_sys_end[];


FIASCO_INIT
void Kip_init::init()
{
  Kip *kinfo = reinterpret_cast<Kip*>(&KIP_namespace::my_kernel_info_page);

  Kip::init_global_kip(kinfo);

  setup_user_virtual(kinfo);

  reserve_amd64_hole();


  for (auto &md: kinfo->mem_descs_a())
    {
      if (md.type() != Mem_desc::Reserved || md.is_virtual())
	continue;

      if (md.start() == (Address)_boot_sys_start
	  && md.end() == (Address)_boot_sys_end - 1)
	md.type(Mem_desc::Undefined);

      if (md.contains(Kmem::kernel_image_start())
	  && md.contains(Kmem::kcode_end()-1))
	{
	  md = Mem_desc(Kmem::kernel_image_start(), Kmem::kcode_end() -1,
	                Mem_desc::Reserved);
	}
    }
}

#define _STR(v)     __STR(v)
#define __STR(v)    #v

#ifdef CONFIG_BIT32
#define KIP_CLOCK_BXREL(f, o) _STR((o + OFS_KIP_CLOCK - OFS_KIP_CODE_ ##f - (2b - 1b))(%ebx))

asm (
R"( .pushsection ".initcall.text.kcts", "ax"
    _kip_time_code: )"
    KIP_CODE_HDR(1f, 5f, 0, 8f) R"(
    1: push    %ebx
       call    2f
    2: pop     %ebx
    3: movl    )" KIP_CLOCK_BXREL(READ_US, 4) R"(, %edx
       movl    )" KIP_CLOCK_BXREL(READ_US, 0) R"(, %eax
       cmpl    %edx, )" KIP_CLOCK_BXREL(READ_US, 4) R"(
       jne     3b
       pop     %ebx
       ret
    5:
    8: )"
    KIP_CODE_HDR(1f, 5f, 0x80, 8b) R"(
    1: push    %ebx
       call    2f
    2: pop     %ebx
    3: movl    )"  KIP_CLOCK_BXREL(READ_NS, 4) R"(, %edx
       movl    )"  KIP_CLOCK_BXREL(READ_NS, 0) R"(, %eax
       cmpl    %edx, )" KIP_CLOCK_BXREL(READ_NS, 4) R"(
       jne     3b
       movl    $1000, %ebx
       movl    %eax, %ecx # ecx = clock.lo
       movl    %edx, %eax # eax = clock.hi
       mull    %ebx       # eax = clock.hi * 1000, ignore bits in edx
       xchg    %ecx, %eax # eax = clock.lo, ecx = (clock.hi * 1000).lo
       mull    %ebx       # edx:eax = clock.lo * 1000
       addl    %ecx, %edx # (clock.lo * 1000).hi += (clock.hi * 1000).lo
       pop     %ebx
       ret
    5:
    .popsection
)");
#endif

#ifdef CONFIG_BIT64
#define KIP_CLOCK_RIPREL(f, o) _STR((1b - o + OFS_KIP_CLOCK - OFS_KIP_CODE_ ##f)(%rip))

asm (
R"( .pushsection ".initcall.text", "ax"
    _kip_time_code: )"
    KIP_CODE_HDR(1f, 5f, 0, 8f) R"(
    1: movq  )" KIP_CLOCK_RIPREL(READ_US, 0) R"(, %rax
       ret
    5:
    8: )"
    KIP_CODE_HDR(1f, 5f, 0x80, 8b) R"(
    1: movq  )" KIP_CLOCK_RIPREL(READ_NS, 0) R"(, %rax
       movl    $1000, %edx
       mulq    %rdx
       ret
    5:
    .popsection
)");
#endif

extern unsigned char const _kip_time_code[];

FIASCO_INIT
void
Kip_init::init_kip_clock()
{
  kip_clock_deploy_code_blob(Kip::k(), _kip_time_code);
}

