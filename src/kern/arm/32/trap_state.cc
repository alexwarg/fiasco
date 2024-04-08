
#include "trap_state.h"
#include "mem_layout.h"

#include <cstdio>

void
Trap_state::dump() const
{
  char const *excpts[] =
    {/*  0 */ "undef insn",  "WFx",        0,            "MCR (CP15)",
     /*  4 */ "MCRR (CP15)", "MCR (CP14)", "LDC (CP14)", "coproc trap",
     /*  8 */ "MRC (CP10)",  0,            "BXJ",        0,
     /*  C */ "MRRC (CP14)", 0,            0,            0,
     /* 10 */ 0,             "SVC",        "HVC",        "SMC",
     /* 14 */ 0, 0, 0, 0,
     /* 18 */ 0, 0, 0, 0,
     /* 1C */ 0, 0, 0, 0,
     /* 20 */ "prefetch abt (usr)", "prefetch abt (kernel)", 0, 0,
     /* 24 */ "data abt (user)",    "data abt (kernel)",     0, 0,
     /* 28 */ 0, 0, 0, 0,
     /* 2C */ 0, 0, 0, 0,
     /* 30 */ 0, 0, 0, 0,
     /* 34 */ 0, 0, 0, 0,
     /* 38 */ 0, 0, 0, 0,
     /* 3C */ 0, 0, "<TrExc>", "<IPC>"};

  printf("EXCEPTION: (%02x) %s pfa=%08lx, error=%08lx psr=%08lx\n",
         (unsigned)esr.ec(), excpts[esr.ec()] ? excpts[esr.ec()] : "",
         pf_address, error_code, psr);

  printf("R[0]: %08lx %08lx %08lx %08lx  %08lx %08lx %08lx %08lx\n"
         "R[8]: %08lx %08lx %08lx %08lx  %08lx %08lx %08lx %08lx\n",
	 r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7],
	 r[8], r[9], r[10], r[11], r[12], usp, ulr, pc);

  extern char virt_address[] asm ("virt_address");
  Mword lower_limit = (Mword)&virt_address;
  Mword upper_limit = (Mword)&Mem_layout::initcall_end;
  if (lower_limit <= pc && pc < upper_limit)
    {
      printf("Data around PC at 0x%lx:\n", pc);
      for (Mword d = pc - 24; d < pc + 28; d += 4)
        if (lower_limit <= d && d < upper_limit)
          printf("%s0x%08lx: %08x\n", d == pc ? "->" : "  ", d,
                                      *reinterpret_cast<unsigned *>(d));
    }
}

