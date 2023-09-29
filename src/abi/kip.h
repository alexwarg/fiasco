#pragma once

#include <cxx/static_vector>
#include <types.h>
#include <panic.h>
#include <config.h>
#include <mem.h>

#include "kip_arch.h"

class Mem_desc
{
public:
  enum Mem_type
  {
    Undefined    = 0x0,
    Conventional = 0x1,
    Reserved     = 0x2,
    Dedicated    = 0x3,
    Shared       = 0x4,
    Kernel_tmp   = 0x7,

    Info         = 0xd,
    Bootloader   = 0xe,
    Arch         = 0xf,
  };

  enum Ext_type_info
  {
    Info_acpi_rsdp = 0
  };

  constexpr Mem_desc(Address start, Address end, Mem_type t,
                     bool v = false, unsigned st = 0) noexcept
  : _l((start & ~0x3ffUL) | (t & 0x0f) | ((st << 4) & 0x0f0)
       | (v?0x0200:0x0)),
    _h(end)
  {}

  Address start() const noexcept
  { return _l & ~0x3ffUL; }

  Address end() const noexcept
  { return _h | 0x3ffUL; }

  Address size() const noexcept
  { return end() - start() + 1; }

  void type(Mem_type t) noexcept
  { _l = (_l & ~0x0f) | (t & 0x0f); }

  Mem_type type() const noexcept
  { return (Mem_type)(_l & 0x0f); }

  unsigned ext_type() const noexcept
  { return (_l >> 4) & 0x0f; }

  unsigned is_virtual() const noexcept
  { return _l & 0x200; }

  bool contains(Address addr) const noexcept
  {
    return start() <= addr && end() >= addr;
  }

  bool valid() const noexcept
  { return type() && start() < end(); }

  // only availabe with debugger
  void dump() const;

private:
  Mword _l, _h;
};

class Kip
{
public:
  using Platform_info = Kip_arch_platform_info;

  enum : Cpu_time
  {
    Clock_1_second = 1000000ULL,
    Clock_1_hour   = 3600ULL * Clock_1_second,
    Clock_1_day    = 24ULL * Clock_1_hour,
    Clock_1_year   = 365UL * Clock_1_day,
  };

  char const *version_string() const;

  Cpu_time clock() const
  {
    return Mem::read64_consistent(const_cast<Cpu_time const *>(&_clock));
  }

  void set_clock(Cpu_time c)
  { _clock = c; }

  void add_to_clock(Cpu_time plus)
  {
    // This function does not force a full atomic update. The caller needs to be
    // aware about this: Either the update is performed by a single specific CPU
    // (the boot CPU) or the callers have to use a lock.
    // However, on ARM < v7, the update of the 64-bit clock can be observed in
    // any order defeating the user-level retry loop for reading the clock which
    // assumes that low word is written before the high word.
    Mem::write64_consistent(const_cast<Cpu_time *>(&_clock), _clock + plus);
  }

  /* 0x00 */
  Mword      magic;
  Mword      version;
  Unsigned8  offset_version_strings;
  Unsigned8  fill0[sizeof(Mword) - 1];
  Unsigned8  kip_sys_calls;
  Unsigned8  fill1[sizeof(Mword) - 1];

  /* the following stuff is undocumented; we assume that the kernel
     info page is located at offset 0x1000 into the L4 kernel boot
     image so that these declarations are consistent with section 2.9
     of the L4 Reference Manual */

  /* 0x10   0x20 */
  Mword      sched_granularity;
  Mword      _res1[3];

  /* 0x20   0x40 */
  Mword      sigma0_sp, sigma0_ip;
  Mword      _res2[2];

  /* 0x30   0x60 */
  Mword      sigma1_sp, sigma1_ip;
  Mword      _res3[2];

  /* 0x40   0x80 */
  Mword      root_sp, root_ip;
  Mword      _res4[2];

  /* 0x50   0xA0 */
  Mword      _res_50;
  Mword      _mem_info;
  Mword      _res_58[2];

  /* 0x60   0xC0 */
  Mword      _res5[16];

  /* 0xA0   0x140 */
  volatile Cpu_time _clock; // don't access directly, use clock() instead!
  Unsigned64 _res6;  // might be later used for clock-related time stamp offset

  /* 0xB0   0x150 */
  Mword      frequency_cpu;
  Mword      frequency_bus;

  /* 0xB8   0x160 */
  Mword      _res7[10 + ((sizeof(Mword) == 8) ? 2 : 0)];

  /* 0xE0   0x1C0 */
  Mword      user_ptr;
  Mword      vhw_offset;
  Mword      _res8[2];

  /* 0xF0   0x1E0 */
  Platform_info platform_info;

  /* 0xF0 + sizeof(Platform_info) / 0x1E0 + sizeof(Platform_info):
   * - Memory descriptors (2 Mwords per descriptor),
   * - kernel version string ('\0'-terminated),
   * - feature strings ('\0'-terminated)
   * - terminating '\0'. */

  /* 0x800-0x900:
   * Code for syscall invocation. */

  /* 0x900-0x97F:
   * Code for reading the system clock with microsecond resolution. Depending on
   * the configuration, this clock might or might not be synchronized with the
   * KIP clock. */

  /* 0x980-0x9FF:
   * Code for reading the system clock with nanosecond resolution. Depending on
   * the configuration, this clock might or might not be synchronized with the
   * KIP clock. */

  char *clock_blob() noexcept
  {
    union U
    {
      Kip k;
      char b[4096];
    };

    return &reinterpret_cast<U *>(this)->b[0x900];
  }

  Mem_desc *mem_descs() noexcept
  { return (Mem_desc*)(((Address)this) + (_mem_info >> (MWORD_BITS/2))); }

  Mem_desc const *mem_descs() const noexcept
  { return (Mem_desc const *)(((Address)this) + (_mem_info >> (MWORD_BITS/2))); }

  unsigned num_mem_descs() const noexcept
  { return _mem_info & ((1UL << (MWORD_BITS/2))-1); }

  cxx::static_vector<Mem_desc> mem_descs_a() noexcept
  { return cxx::static_vector<Mem_desc>(mem_descs(), num_mem_descs()); }

  cxx::static_vector<Mem_desc const> mem_descs_a() const noexcept
  { return cxx::static_vector<Mem_desc const>(mem_descs(), num_mem_descs()); }

  void num_mem_descs(unsigned n) noexcept
  {
    _mem_info = (_mem_info & ~((1UL << (MWORD_BITS / 2)) - 1))
                | (n & ((1UL << (MWORD_BITS / 2)) - 1));
  }

  Mem_desc *add_mem_region(Mem_desc const &md)
  {
    for (auto &m: mem_descs_a())
      if (m.type() == Mem_desc::Undefined)
        {
          m = md;
          return &m;
        }

    // Add mem region failed -- must be a Fiasco startup problem.  Bail out.
    panic("Too few memory descriptors in KIP");

    return 0;
  }

  static void init_global_kip(Kip *kip)
  {
    global_kip = kip;

    kip->platform_info.is_mp = Config::Max_num_cpus > 1;
    kip->sched_granularity = Config::Scheduler_granularity;

    // check that the KIP has actually been set up
    //assert(kip->sigma0_ip && kip->root_ip && kip->user_ptr);
  }

  [[gnu::pure]] static Kip *k()
  { return global_kip; }

  // only available with debugger
  void print() const;

private:
  static Kip *global_kip asm ("GLOBAL_KIP");

  // only available with debugger
  void debug_print_syscalls() const;
  void debug_print_features() const;
  void debug_print_memory() const;

  
};

#define L4_KERNEL_INFO_MAGIC (0x4BE6344CL) /* "L4µK" */

