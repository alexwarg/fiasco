
#include "trap_state.h"


#include <cstdio>
#include <panic.h>
#include <cxx/atomic>

#include "cpu.h"
#include "gdt.h"
#include "regdefs.h"
#include "mem.h"

Trap_state::Handler Trap_state::base_handler FIASCO_FASTCALL;

void
Trap_state::dump() const
{
  int from_user = _cs & 3;

  printf("RAX %016lx    RBX %016lx\n", _ax, _bx);
  printf("RCX %016lx    RDX %016lx\n", _cx, _dx);
  printf("RSI %016lx    RDI %016lx\n", _si, _di);
  printf("RBP %016lx    RSP %016lx\n", _bp, from_user ? _sp : (Address)&_sp);
  printf("R8  %016lx    R9  %016lx\n", _r8,  _r9);
  printf("R10 %016lx    R11 %016lx\n", _r10, _r11);
  printf("R12 %016lx    R13 %016lx\n", _r12, _r13);
  printf("R14 %016lx    R15 %016lx\n", _r14, _r15);
  printf("RIP %016lx RFLAGS %016lx\n", _ip, _flags);
  printf("CS %04lx SS %04lx\n", _cs, _ss);
  printf("\n");
  printf("trapno %lu, error %lx, from %s mode\n",
         _trapno, _err, from_user ? "user" : "kernel");

  if (_trapno == 13)
    {
      if (_err & 1)
	printf("(external event");
      else
	printf("(internal event");
      if (_err & 2)
	{
	  printf(" regarding IDT gate descriptor no. 0x%02lx)\n", _err >> 3);
	}
      else
	{
	  printf(" regarding %s entry no. 0x%02lx)\n",
	      _err & 4 ? "LDT" : "GDT", _err >> 3);
	}
    }
  else if (_trapno == 14)
    printf("page fault linear address %16lx\n", _cr2);
}

extern "C" FIASCO_FASTCALL
void trap_dump_panic(Trap_state *ts);

extern "C" FIASCO_FASTCALL
void
trap_dump_panic(Trap_state *ts)
{
  ts->dump();
  panic("terminated due to trap");
}
