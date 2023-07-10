#pragma once

#include "types.h"
#include "static_assert.h"
#include <mem_layout-ia32-bits.h>

class Mem_layout_arch : public Mem_layout_ia32_bits
{
public:
  enum { Io_port_max = (1UL << 16) };

  static Address _io_map_ptr;

  static inline Address alloc_io_vmem(unsigned long bytes)
  {
    bytes = (bytes + Config::PAGE_SIZE - 1) & ~(Config::PAGE_SIZE - 1);
    if (_io_map_ptr - bytes < Registers_map_start)
      return 0;

    _io_map_ptr -= bytes;
    return _io_map_ptr;
  }

#ifdef CONFIG_VIRT_OBJ_SPACE
  template<typename V>
  static inline bool read_special_safe(V const *address, V &v)
  {
    // Counterpart: Thread::pagein_tcb_request()
    static_assert(sizeof(v) <= sizeof(Mword), "wrong sized argument");
    Mword value;
    bool res;
    asm volatile ("clc; mov (%[adr]), %[val]; setnc %b[ex] \n"
        : [val] "=acd" (value), [ex] "=r" (res)
        : [adr] "acdbSD" (address)
        : "cc");
    v = V(value);
    return res;
  }

  template<typename T>
  static inline T read_special_safe(T const *a)
  {
    // Counterpart: Thread::pagein_tcb_request()
    static_assert(sizeof(T) <= sizeof(Mword), "wrong sized return type");
    Mword res;
    asm volatile ("mov (%1), %0 \n\t"
        : "=acd" (res) : "acdbSD" (a) : "cc");
    return T(res);
  }
#endif
};
