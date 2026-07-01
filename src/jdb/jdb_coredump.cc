/*
 * jdb_coredump.cc -- JDB command "D": emit a GDB-compatible ELF core dump of
 * the current (or a specified) thread, base64-encoded on the JDB console.
 *
 * Usage inside JDB:
 *   D        -- dump the thread that triggered the debugger
 *   D<tid>   -- dump an arbitrary thread by its thread ID
 *
 * Decode and load:
 *   base64 -d <<'EOF' > core && gdb fiasco core
 *   ===COREDUMP BEGIN===
 *   ...
 *   ===COREDUMP END===
 *   EOF
 *
 * ELF layout:
 *   Ehdr
 *   Phdr[0]  PT_NOTE  -- NT_PRSTATUS note (registers)
 *   Phdr[1]  PT_LOAD  -- kernel stack (ksp..TCB_top), vaddr=ksp
 *   [NOTE data]
 *   [STACK data -- read directly from TCB memory, no intermediate copy]
 *
 * No large intermediate buffer: each piece is fed to a 3-byte carry-based
 * base64 state machine and flushed straight to the JDB console.
 *
 * Architecture-specific parts (Elf_Greg, ELF_MACHINE, ELF_CLASS, fill_greg)
 * live in per-arch headers:
 *   jdb/arm/jdb_coredump_arch.h   -- AArch32 + AArch64
 *   jdb/ia32/jdb_coredump_arch.h  -- i386 + x86-64
 *   jdb/mips/jdb_coredump_arch.h  -- MIPS32 + MIPS64
 */

#include <cstdio>
#include <cstring>

#include "jdb.h"
#include "jdb_module.h"
#include "jdb_kobject.h"
#include "simpleio.h"
#include "static_init.h"
#include "thread.h"
#include "types.h"

// Pulls in Elf_Greg, ELF_MACHINE, ELF_CLASS, fill_greg()
// and fill_greg_from_switch_frame() for the current architecture.
#include <jdb_coredump_arch.h>

// ============================================================
//  Minimal self-contained ELF types
// ============================================================

typedef unsigned char      Elf_Byte;
typedef unsigned short     Elf32_Half;
typedef unsigned int       Elf32_Word;
typedef unsigned int       Elf32_Off;
typedef unsigned int       Elf32_Addr;
typedef unsigned long long Elf64_Xword;
typedef unsigned long long Elf64_Off;
typedef unsigned long long Elf64_Addr;
typedef unsigned int       Elf64_Word;
typedef unsigned short     Elf64_Half;

enum
{
  ELFMAG0     = 0x7f, ELFMAG1 = 'E', ELFMAG2 = 'L', ELFMAG3 = 'F',
  ELFCLASS32  = 1, ELFCLASS64 = 2, ELFDATA2LSB = 1,
  ET_CORE     = 4, PT_LOAD = 1, PT_NOTE = 4,
  PF_R        = 4, PF_W   = 2,
  NT_PRSTATUS = 1, EI_NIDENT = 16,
};

struct Elf32_Ehdr
{
  Elf_Byte   e_ident[EI_NIDENT];
  Elf32_Half e_type, e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off  e_phoff, e_shoff;
  Elf32_Word e_flags;
  Elf32_Half e_ehsize, e_phentsize, e_phnum;
  Elf32_Half e_shentsize, e_shnum, e_shstrndx;
} __attribute__((packed));

struct Elf32_Phdr
{
  Elf32_Word p_type, p_offset;
  Elf32_Addr p_vaddr, p_paddr;
  Elf32_Word p_filesz, p_memsz, p_flags, p_align;
} __attribute__((packed));

struct Elf64_Ehdr
{
  Elf_Byte   e_ident[EI_NIDENT];
  Elf64_Half e_type, e_machine;
  Elf64_Word e_version;
  Elf64_Addr e_entry;
  Elf64_Off  e_phoff, e_shoff;
  Elf64_Word e_flags;
  Elf64_Half e_ehsize, e_phentsize, e_phnum;
  Elf64_Half e_shentsize, e_shnum, e_shstrndx;
} __attribute__((packed));

struct Elf64_Phdr
{
  Elf64_Word  p_type, p_flags;
  Elf64_Off   p_offset;
  Elf64_Addr  p_vaddr, p_paddr;
  Elf64_Xword p_filesz, p_memsz, p_align;
} __attribute__((packed));

struct Elf_Nhdr
{
  Elf32_Word n_namesz, n_descsz, n_type;
} __attribute__((packed));

#ifdef CONFIG_BIT64
typedef Elf64_Ehdr Elf_Ehdr;
typedef Elf64_Phdr Elf_Phdr;
#else
typedef Elf32_Ehdr Elf_Ehdr;
typedef Elf32_Phdr Elf_Phdr;
#endif

// ============================================================
//  NT_PRSTATUS descriptor
//
//  Must match the natural (non-packed) C layout of Linux's
//  struct elf_prstatus exactly -- GDB uses hardcoded field offsets.
//  The 2-byte gap after pr_cursig (to align unsigned long to 8/4 bytes)
//  is load-bearing; __attribute__((packed)) would shift pr_reg by 2 bytes
//  and silently break all register decoding.
// ============================================================

struct Elf_Prstatus
{
  int           pr_info_signo, pr_info_code, pr_info_errno;
  short         pr_cursig;
  // natural 2-byte pad -- NOT packed
  unsigned long pr_sigpend, pr_sighold;
  int           pr_pid, pr_ppid, pr_pgrp, pr_sid;
  long          pr_utime[2], pr_stime[2], pr_cutime[2], pr_cstime[2];
  Elf_Greg      pr_reg;
  int           pr_fpvalid;
};

// ============================================================
//  Streaming base64 encoder
//
//  Feed arbitrary-length byte chunks via write().  A 2-byte carry buffer
//  absorbs chunk boundaries that are not multiples of 3.
//  flush() emits the final partial group with '=' padding.
//  Lines are 76 chars wide (57 input bytes per line, MIME standard).
// ============================================================

struct B64
{
  unsigned char carry[3];
  unsigned      carry_n;  // 0, 1, or 2
  unsigned      col;      // input bytes consumed on the current line (0..56)

  static constexpr unsigned LINE_IN = 57;

  static char enc(unsigned v)
  {
    static char const a[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    return a[v & 0x3f];
  }

  void emit3(unsigned b0, unsigned b1, unsigned b2)
  {
    putchar(enc(b0 >> 2));
    putchar(enc(((b0 & 3) << 4) | (b1 >> 4)));
    putchar(enc(((b1 & 0xf) << 2) | (b2 >> 6)));
    putchar(enc(b2));
    col += 3;
    if (col >= LINE_IN) { putchar('\n'); col = 0; }
  }

  void init() { carry_n = 0; col = 0; }

  void write(void const *data, unsigned len)
  {
    unsigned char const *p = static_cast<unsigned char const *>(data);
    unsigned i = 0;

    // Top up carry to 3 bytes, emit if complete
    while (carry_n < 3 && i < len)
      carry[carry_n++] = p[i++];

    if (carry_n == 3)
      { emit3(carry[0], carry[1], carry[2]); carry_n = 0; }
    else
      return; // still starved (i == len)

    // Full triples direct from input
    while (i + 2 < len)
      { emit3(p[i], p[i+1], p[i+2]); i += 3; }

    // Stash remainder
    while (i < len)
      carry[carry_n++] = p[i++];
  }

  void write_zeros(unsigned n)
  {
    static unsigned char const z[3] = {};
    while (n >= 3) { write(z, 3); n -= 3; }
    if (n)          write(z, n);
  }

  void flush()
  {
    if (carry_n)
      {
        unsigned b0 = carry[0];
        unsigned b1 = (carry_n > 1) ? carry[1] : 0u;
        putchar(enc(b0 >> 2));
        putchar(enc(((b0 & 3) << 4) | (b1 >> 4)));
        putchar((carry_n > 1) ? enc((b1 & 0xf) << 2) : '=');
        putchar('=');
        carry_n = 0;
      }
    // always terminate the last line, whether it came from emit3 or here
    if (col) putchar('\n');
    col = 0;
  }

  void write_ehdr(unsigned phnum, unsigned phdr_sz, unsigned ehdr_sz)
  {
    Elf_Ehdr eh;
    memset(&eh, 0, sizeof(eh));
    eh.e_ident[0] = ELFMAG0; eh.e_ident[1] = ELFMAG1;
    eh.e_ident[2] = ELFMAG2; eh.e_ident[3] = ELFMAG3;
    eh.e_ident[4] = ELF_CLASS; eh.e_ident[5] = ELFDATA2LSB; eh.e_ident[6] = 1;
    eh.e_type      = ET_CORE;
    eh.e_machine   = ELF_MACHINE;
    eh.e_version   = 1;
    eh.e_phoff     = ehdr_sz;
    eh.e_ehsize    = ehdr_sz;
    eh.e_phentsize = phdr_sz;
    eh.e_phnum     = phnum;
    write(&eh, sizeof(eh));
  }

  void write_note_phdr(unsigned offset, unsigned size)
  {
    Elf_Phdr ph;
    memset(&ph, 0, sizeof(ph));
    ph.p_type   = PT_NOTE;
    ph.p_flags  = PF_R;
    ph.p_offset = offset;
    ph.p_filesz = size;
    ph.p_memsz  = size;
    ph.p_align  = 4;
    write(&ph, sizeof(ph));
  }

  void write_load_phdr(unsigned offset, Address vaddr,
                       unsigned size, unsigned flags)
  {
    Elf_Phdr ph;
    memset(&ph, 0, sizeof(ph));
    ph.p_type   = PT_LOAD;
    ph.p_flags  = flags;
    ph.p_offset = offset;
    ph.p_vaddr  = vaddr;
    ph.p_paddr  = vaddr;
    ph.p_filesz = size;
    ph.p_memsz  = size;
    ph.p_align  = sizeof(Mword);
    write(&ph, sizeof(ph));
  }
};

// ============================================================
//  ELF core builder -- streaming, no large intermediate buffer
// ============================================================

static void build_and_emit(Thread *t, bool is_current, Jdb_entry_frame *ef)
{
  // -- Kernel stack extent --------------------------------------------------
  // switch_ksp: raw saved kernel_sp; valid for any thread at any time.
  // For the currently trapped thread we use ef->ksp() as the stack top
  // (it points at the entry frame, which is higher than switch_ksp).
  Mword  *switch_ksp = t->get_kernel_sp();
  Address ksp_val    = is_current && ef
                         ? ef->ksp()
                         : reinterpret_cast<Address>(switch_ksp);

  Address tcb_base = reinterpret_cast<Address>(
    context_of(reinterpret_cast<void *>(ksp_val)));
  Address tcb_top  = tcb_base + Context::Size;

  if (ksp_val < tcb_base || ksp_val >= tcb_top)
    ksp_val = tcb_top;  // degenerate: emit zero-byte stack segment

  unsigned stack_bytes = static_cast<unsigned>(tcb_top - ksp_val);

  // -- Note sizes -----------------------------------------------------------
  static constexpr char     NOTE_NAME[] = "CORE"; // 5 bytes incl. NUL
  static constexpr unsigned NAME_SZ     = sizeof(NOTE_NAME);
  static constexpr unsigned NAME_PAD    = (4u - (NAME_SZ & 3u)) & 3u;
  static constexpr unsigned DESC_SZ     = sizeof(Elf_Prstatus);
  static constexpr unsigned DESC_PAD    = (4u - (DESC_SZ & 3u)) & 3u;
  static constexpr unsigned NOTE_TOTAL  =
    sizeof(Elf_Nhdr) + NAME_SZ + NAME_PAD + DESC_SZ + DESC_PAD;

  // -- Thread struct extent -------------------------------------------------
  unsigned const tcb_bytes = sizeof(Thread);

  // -- File offsets ---------------------------------------------------------
  static constexpr unsigned EHDR_SZ  = sizeof(Elf_Ehdr);
  static constexpr unsigned PHDR_SZ  = sizeof(Elf_Phdr);
  static constexpr unsigned NOTE_OFF  = EHDR_SZ + 3 * PHDR_SZ;
  static constexpr unsigned STACK_OFF = NOTE_OFF + NOTE_TOTAL;
  unsigned const             TCB_OFF  = STACK_OFF + stack_bytes;

  // -- Register block + prstatus (on the JDB stack, < 500 bytes) ------------
  Elf_Prstatus ps;
  memset(&ps, 0, sizeof(ps));
  ps.pr_info_signo = 11; // SIGSEGV -- generic "stopped" marker for GDB
  ps.pr_cursig     = 11;
  ps.pr_pid = static_cast<int>(reinterpret_cast<Address>(t) & 0x7fffffff);
  fill_greg(&ps.pr_reg, ef, switch_ksp);

  Elf_Nhdr nh;
  nh.n_namesz = NAME_SZ;
  nh.n_descsz = DESC_SZ;
  nh.n_type   = NT_PRSTATUS;

  // -- Stream everything through the base64 encoder -------------------------
  static B64 b64; // static: keeps it off the limited JDB call stack
  b64.init();

  puts("\n===COREDUMP BEGIN===");

  b64.write_ehdr(3, PHDR_SZ, EHDR_SZ);
  b64.write_note_phdr(NOTE_OFF, NOTE_TOTAL);
  b64.write_load_phdr(STACK_OFF, ksp_val,  stack_bytes, PF_R | PF_W);
  b64.write_load_phdr(TCB_OFF,   tcb_base, tcb_bytes,   PF_R);

  // Note data
  b64.write(&nh, sizeof(nh));
  b64.write(NOTE_NAME, NAME_SZ);
  b64.write_zeros(NAME_PAD);
  b64.write(&ps, DESC_SZ);
  b64.write_zeros(DESC_PAD);

  // Stack segment -- read directly from TCB memory, no intermediate copy
  if (stack_bytes)
    b64.write(reinterpret_cast<void *>(ksp_val), stack_bytes);

  // Thread struct -- the fixed-size object at the base of the TCB page
  b64.write(reinterpret_cast<void *>(tcb_base), tcb_bytes);

  b64.flush();
  puts("===COREDUMP END===\n");
}

// ============================================================
//  JDB module registration
// ============================================================

class Jdb_coredump : public Jdb_module
{
public:
  Jdb_coredump() FIASCO_INIT;

  Action_code action(int cmd, void *&args, char const *&fmt,
                     int &next_char) override;
  Cmd const  *cmds()     const override;
  int         num_cmds() const override;

private:
  static char     first_char;
  static Kobject *threadid;
};

char     Jdb_coredump::first_char;
Kobject *Jdb_coredump::threadid;

Jdb_module::Action_code
Jdb_coredump::action(int cmd, void *&args, char const *&fmt, int &next_char)
{
  if (cmd != 0)
    return NOTHING;

  if (args == &first_char)
    {
      if (first_char == '\r' || first_char == ' ')
        {
          Cpu_number cpu = Jdb::triggered_on_cpu;
          Thread *t = Jdb::get_thread(cpu);
          if (!t) { puts(" no current thread"); return NOTHING; }
          build_and_emit(t, /*is_current=*/true, Jdb::get_entry_frame(cpu));
          return NOTHING;
        }
      // Start of a thread id -- hand the first character to the %q parser
      args      = &threadid;
      fmt       = "%q";
      next_char = first_char;
      return EXTRA_INPUT_WITH_NEXTCHAR;
    }

  Thread *t = cxx::dyn_cast<Thread *>(threadid);
  if (!t) { puts(" invalid thread"); return NOTHING; }

  Cpu_number cpu    = Jdb::triggered_on_cpu;
  bool       is_cur = (t == Jdb::get_thread(cpu));
  build_and_emit(t, is_cur, is_cur ? Jdb::get_entry_frame(cpu) : nullptr);
  return NOTHING;
}

Jdb_module::Cmd const *Jdb_coredump::cmds() const
{
  static Cmd cs[] =
    {
      { 0, "D", "coredump", "%C",
        "D[<thread>]\tELF core dump of TCB+stack, base64 on console",
        &first_char },
    };
  return cs;
}

int Jdb_coredump::num_cmds() const { return 1; }

Jdb_coredump::Jdb_coredump() : Jdb_module("INFO") {}

static Jdb_coredump jdb_coredump INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
