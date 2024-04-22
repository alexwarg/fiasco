#pragma once

#include <globalconfig.h>

#ifdef CONFIG_ARM_CACHE_L2CXX0
#include <outer_cache-l2cxx0.h>

namespace Outer_cache
{
  inline void
  invalidate(Address start, Address end, bool do_sync = true)
  {
    if (start & Cache_line_mask)
      {
        flush(start, false);
        start += Cache_line_size;
      }
    if (end & Cache_line_mask)
      {
        flush(end, false);
        end &= ~Cache_line_mask;
      }

    for (Address a = start & ~Cache_line_mask; a < end; a += Cache_line_size)
      invalidate(a, false);

    if (do_sync)
      sync();
  }

  inline void
  clean(Address start, Address end, bool do_sync = true)
  {
    for (Address a = start & ~Cache_line_mask;
         a < end; a += Cache_line_size)
      clean(a, false);
    if (do_sync)
      sync();
  }

  inline void
  flush(Address start, Address end, bool do_sync = true)
  {
    for (Address a = start & ~Cache_line_mask;
         a < end; a += Cache_line_size)
      flush(a, false);
    if (do_sync)
      sync();
  }
} // namespace

#else // CONFIG_ARM_OUTER_CACHE

namespace Outer_cache
{
  inline void invalidate() {}
  inline void invalidate(Address, bool = true) {}
  inline void clean() {}
  inline void clean(Address, bool = true) {}
  inline void flush() {}
  inline void flush(Address, bool = true) {}
  inline void sync() {}
  inline void invalidate(Address, Address, bool = true) {}
  inline void clean(Address, Address, bool = true) {}
  inline void flush(Address, Address, bool = true) {}
} // namesapce

#endif // CONFIG_ARM_OUTER_CACHE
