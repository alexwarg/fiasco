
#include "trap_state.h"
#include "mem_layout.h"

#include <cstdio>

void
Trap_state::dump() const
{
  char const *excpts[] =
    {/*  0 */ "undef insn",  "WFx",        nullptr,      "MCR (CP15)",
     /*  4 */ "MCRR (CP15)", "MCR (CP14)", "LDC (CP14)", "coproc trap",
     /*  8 */ "MRC (CP10)",  nullptr,      "BXJ",        nullptr,
     /*  C */ "MRRC (CP14)", nullptr,      nullptr,      nullptr,
     /* 10 */ nullptr,       "SVC",        "HVC",        "SMC",
     /* 14 */ nullptr, nullptr, nullptr, nullptr,
     /* 18 */ nullptr, nullptr, nullptr, nullptr,
     /* 1C */ nullptr, nullptr, nullptr, nullptr,
     /* 20 */ "prefetch abt (usr)", "prefetch abt (kernel)", nullptr, nullptr,
     /* 24 */ "data abt (user)",    "data abt (kernel)",     nullptr, nullptr,
     /* 28 */ nullptr, nullptr, nullptr, nullptr,
     /* 2C */ nullptr, nullptr, nullptr, nullptr,
     /* 30 */ nullptr, nullptr, nullptr, nullptr,
     /* 34 */ nullptr, nullptr, nullptr, nullptr,
     /* 38 */ nullptr, nullptr, nullptr, nullptr,
     /* 3C */ nullptr, nullptr, "<TrExc>", "<IPC>"};

  printf("EXCEPTION: (%02x) %s pfa=%08lx, error=%08lx psr=%08lx\n",
         static_cast<unsigned>(esr.ec()), excpts[esr.ec()] ? excpts[esr.ec()] : "",
         pf_address, error_code, psr);

  printf("R[0]: %08lx %08lx %08lx %08lx  %08lx %08lx %08lx %08lx\n"
         "R[8]: %08lx %08lx %08lx %08lx  %08lx %08lx %08lx %08lx\n",
	 r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7],
	 r[8], r[9], r[10], r[11], r[12], usp, ulr, pc);

  extern char virt_address[] asm ("virt_address");
  Mword lower_limit = reinterpret_cast<Mword>(&virt_address);
  Mword upper_limit = reinterpret_cast<Mword>(&Mem_layout::initcall_end);
  if (lower_limit <= pc && pc < upper_limit)
    {
      printf("Data around PC at 0x%lx:\n", pc);
      for (Mword d = pc - 24; d < pc + 28; d += 4)
        if (lower_limit <= d && d < upper_limit)
          printf("%s0x%08lx: %08x\n", d == pc ? "->" : "  ", d,
                                      *reinterpret_cast<unsigned *>(d));
    }
}

