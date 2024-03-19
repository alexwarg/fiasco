#pragma once

#include <cstddef>
#include <cxx/slist>
#include <cxx/type_traits>

class Boot_alloced
{
private:
  enum { Debug_boot_alloc };
  struct Block : cxx::S_list_item
  { size_t size; };

  typedef cxx::S_list_bss<Block> Block_list;

  static Block_list _free;

public:
  static void *alloc(size_t size);

  template<typename T> static
  T *allocate(size_t count = 1)
  {
    return reinterpret_cast<T *>(alloc(count * sizeof(T)));
  }

  void *operator new (size_t size) noexcept
  { return alloc(size); }

  void *operator new [] (size_t size) noexcept
  { return alloc(size); }

  void operator delete (void *b);
  void operator delete [] (void *b);
};

template< typename Base >
class Boot_object : public Base, public Boot_alloced
{
public:
  Boot_object()  = default;

  template< typename... A >
  Boot_object(A&&... args) : Base(cxx::forward<A>(args)...) {}
};

