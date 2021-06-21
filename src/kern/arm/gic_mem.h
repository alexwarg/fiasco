#pragma once

#include <mem.h>
#include <mem_unit.h>

#include <cassert>

/**
 * The GIC has strict alignment requirements for certain memory areas that the
 * kernel has to allocate for the GIC. In addition, the kernel has to discover
 * the cacheability and shareability memory attributes supported by the GIC for
 * such a memory area. After writing to a memory area, depending on its memory
 * attributes, the kernel may need to perform cache maintenance operations to
 * ensure that all changes are observable by the GIC.
 */
class Gic_mem
{
  enum
  {
    Shareability_non_shareable    = 0,
    Shareability_inner_shareable  = 1,

    Cacheability_non_cacheable    = 1,
    Cacheability_cacheable_rawawb = 7,
  };

public:
  class Mem_chunk
  {
  public:
    Mem_chunk() = default;

    inline Mem_chunk(void *va, unsigned size)
    : _va(va), _size(size)
    {}

    template<typename R, typename T>
    inline void setup_reg(R reg, T val)
    {
      val.shareability() = _share;
      val.cacheability() = _cache;
      reg.write(val.raw);

      T rval(reg.read());
      _cache = rval.cacheability();
      if (rval.shareability() != _share)
      {
        // Inner shareable not supported by the GIC
        _share = rval.shareability();
        if (_share == Shareability_non_shareable)
          {
            // Mark memory non-cacheable if GIC only supports non-shareable
            _cache = Cacheability_non_cacheable;
            rval.cacheability() = _cache;
            reg.write(rval.raw);
          }
      }
    }

    /**
     * Ensure that all changes are observable by the GIC.
     */
    inline void make_coherent(void *start = nullptr, void *end = nullptr) const
    {
      if (_share != Shareability_inner_shareable)
        {
          if (start == nullptr || end == nullptr)
            Mem_unit::flush_dcache(virt_ptr(), virt_ptr<Unsigned8>() + _size);
          else
            {
              assert(start >= virt_ptr());
              assert(end < (virt_ptr<Unsigned8>() + _size));

              Mem_unit::flush_dcache(start, end);
            }
        }
      else
        Mem::dsbst();
    }

    inline void inherit_mem_attribs(Mem_chunk const &mem)
    {
      _share = mem._share;
      _cache = mem._cache;
    }

    inline bool is_valid() const
    { return _va != nullptr; }

    inline Address virt_addr() const
    { return reinterpret_cast<Address>(_va); }

    template<typename T = void>
    inline T *virt_ptr() const
    { return static_cast<T *>(_va); }

    inline Address phys_addr() const
    { return to_phys(virt_addr()); }

    inline void free()
    {
      if (is_valid())
        {
          free_mem(_va, _size);
          _va = nullptr;
          _size = 0;
        }
    }

  private:
    void *_va = nullptr;
    unsigned _size = 0;
    unsigned _share = Shareability_inner_shareable;
    unsigned _cache = Cacheability_cacheable_rawawb;
  };

  static Mem_chunk alloc_mem(unsigned size, unsigned align = 1);
  static Mem_chunk alloc_zmem(unsigned size, unsigned align = 1);
  static Address to_phys(Address virt);

private:
  static void free_mem(void *mem, unsigned size);
};

