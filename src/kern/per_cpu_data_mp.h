#pragma once

#include <cxx/cxx_int>
#include <new>

#include "per_cpu_array.h"

struct Per_cpu_ctor_data
{
  typedef void (*Func)(void *, Cpu_number);

  void exec(Cpu_number cpu) const
  {
    _func(_base, cpu);
  }

  Per_cpu_ctor_data() = default;
  Per_cpu_ctor_data(Func ctor, void *base)
  : _func(ctor), _base(base)
  {}

private:
  Func _func;
  void *_base;
};


class Per_cpu_data_mp
{
public:
  static void init_ctors();
  static void run_ctors(Cpu_number cpu);
  static void run_late_ctors(Cpu_number cpu);

  static bool valid(Cpu_number cpu) noexcept
  { return cpu < num_cpus() && _offsets[cpu] != -1; }

  enum With_cpu_num { Cpu_num };

  static long offset(Cpu_number cpu)
  {
    return _offsets[cpu];
  }

private:
  typedef Per_cpu_ctor_data Ctor;

  struct Ctor_vector
  {
    void push_back(Ctor::Func func, void *base);
    unsigned len() const { return _len; }
    Ctor const &operator [] (unsigned idx) const
    {
      extern Ctor _per_cpu_ctor_data_start[];
      return _per_cpu_ctor_data_start[idx];
    }

  private:
    unsigned _len;
  };

protected:
  enum { Num_cpus = Config::Max_num_cpus + 1 }; // add one for the never running CPU
  static Cpu_number num_cpus() { return Cpu_number(Num_cpus); }
  typedef Per_cpu_array<long, 1> Offset_array;
  static Offset_array _offsets;
  static unsigned late_ctor_start;
  static Ctor_vector ctors;

  template<typename T>
  static T per_cpu_ref(T adr, Cpu_number cpu)
  {
    return reinterpret_cast<T>(
        const_cast<char *>(
          reinterpret_cast<char const *>(adr) + _offsets[cpu]));
  }

  template<typename T>
  static void ctor_wo_arg(void *obj, Cpu_number cpu)
  {
    //printf("Per_cpu<T>::ctor_wo_arg(obj=%p, cpu=%u -> %p)\n", obj, cpu, &(reinterpret_cast<Per_cpu<T>*>(obj)->cpu(cpu)));
    new (per_cpu_ref(obj, cpu)) T;
  }

  template<typename T>
  static void ctor_w_arg(void *obj, Cpu_number cpu)
  {
    //printf("Per_cpu<T>::ctor_w_arg(obj=%p, cpu=%u -> %p)\n", obj, cpu, &reinterpret_cast<Per_cpu<T>*>(obj)->cpu(cpu));
    new (per_cpu_ref(obj, cpu)) T(cpu);
  }

  template<typename T>
  static void add_ctor_wo_arg(T *o) noexcept
  {
    //printf("  Per_cpu<T>() [this=%p])\n", this);
    ctors.push_back(&ctor_wo_arg<T>, o);
  }

  template<typename T>
  static void add_ctor_w_arg(T *o) noexcept
  {
    //printf("  Per_cpu<T>(bool) [this=%p])\n", this);
    ctors.push_back(&ctor_w_arg<T>, o);
  }
};

using Per_cpu_data = Per_cpu_data_mp;

#define DEFINE_PER_CPU_CTOR_UID(b) __per_cpu_ctor_ ## b
#define DEFINE_PER_CPU_CTOR_DATA(id) \
  __attribute__((section(".bss.per_cpu_ctor_data"),used)) \
    static Per_cpu_ctor_data DEFINE_PER_CPU_CTOR_UID(id);

