#pragma once

#include <globalconfig.h>
#include <mem-arch.h>
#include <types.h>

class Mem_generic_up
{
public:
  inline static void barrier() { __asm__ __volatile__ ("" : : : "memory"); }
  inline static void mb() { barrier(); }
  inline static void rmb() { barrier(); }
  inline static void wmb() { barrier(); }

  inline static void mp_mb() { barrier(); }
  inline static void mp_rmb() { barrier(); }
  inline static void mp_wmb() { barrier(); }
  inline static void mp_acquire() { barrier(); }
  inline static void mp_release() { barrier(); }
};

class Mem_generic_mp
{
public:
  inline static void barrier() { __asm__ __volatile__ ("" : : : "memory"); }
};

template<typename DERIVED>
class Mem_generic
#ifdef CONFIG_MP
  : public Mem_generic_mp
#else
  : public Mem_generic_up
#endif
{
public:
#ifdef CONFIG_BIT64
  template<typename T>
  static inline
  T read64_consistent(T const *t)
  { return access_once(t); }

  template<typename T>
  static inline
  void write64_consistent(T *t, T val)
  { write_now(t, val); }
#endif

#ifdef CONFIG_BIT32
  template<typename T>
  static inline
  T read64_consistent(T const *t)
  {
    static_assert(sizeof(T) == 2* sizeof(Unsigned32), "value has invalid size");
    union U
    {
      T v64;
      struct S32
      {
#ifdef CONFIG_BIG_ENDIAN
        Unsigned32 hi, lo;
#else
        Unsigned32 lo, hi;
#endif
      } v32;
    };

    U const *u = reinterpret_cast<U const *>(t);
    U res;
    do
      {
        res.v32.hi = access_once(&(u->v32.hi));
        DERIVED::mp_rmb();
        res.v32.lo = access_once(&(u->v32.lo));
        DERIVED::mp_rmb();
      }
    while (res.v32.hi != access_once(&(u->v32.hi)));
    return res.v64;
  }

  template<typename T>
  static inline
  void write64_consistent(T *t, T val)
  {
    static_assert(sizeof(T) == 2* sizeof(Unsigned32), "value has invalid size");
    union U
    {
      T v64;
      struct S32
      {
#ifdef CONFIG_BIG_ENDIAN
        Unsigned32 hi, lo;
#else
        Unsigned32 lo, hi;
#endif
      } v32;
    };

    U *u = reinterpret_cast<U *>(t);
    U const &v = reinterpret_cast<U const &>(val);
    write_now(&(u->v32.lo), v.v32.lo);
    DERIVED::mp_wmb();
    write_now(&(u->v32.hi), v.v32.hi);
  }
#endif
};

class Mem : public Mem_arch<Mem, Mem_generic<Mem>>
{
};

