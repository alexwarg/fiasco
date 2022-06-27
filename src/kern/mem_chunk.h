#pragma once

#include <mem.h>
#include <mem_unit.h>

#include <cassert>

class Mem_chunk
{
public:
  Mem_chunk() = default;

  inline Mem_chunk(void *va, unsigned size, unsigned alloc_size)
  : _va(va), _size(size), _alloc_size(alloc_size)
  {}

  inline bool is_valid() const
  { return _va != nullptr; }

  inline Address virt_addr() const
  { return reinterpret_cast<Address>(_va); }

  template<typename T = void>
  inline T *virt_ptr() const
  { return static_cast<T *>(_va); }

  inline Address phys_addr() const
  { return to_phys(virt_addr()); }

  unsigned size() const
  { return _size; }

  inline void free()
  {
    if (is_valid())
      {
        free_mem(_va, _alloc_size);
        _va = nullptr;
        _size = 0;
      }
  }

  template<typename MEM = Mem_chunk>
  static MEM alloc_mem(unsigned size, unsigned align = 1);

  template<typename MEM = Mem_chunk>
  static MEM alloc_zmem(unsigned size, unsigned align = 1);

  static Address to_phys(Address virt);

private:
  static void free_mem(void *mem, unsigned size);

  void *_va = nullptr;
  unsigned _size = 0;
  unsigned _alloc_size = 0;
};
