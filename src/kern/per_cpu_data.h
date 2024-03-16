#pragma once

#include "types.h"
#include "static_init.h"
#include "config.h"
#include "context_base.h"
#include <cxx/type_traits>

#include <cstddef>
#include <new>
#include <construction.h>
#include <cassert>

template< typename T, unsigned EXTRA = 0 >
class Per_cpu_array
: public cxx::array<T, Cpu_number, Config::Max_num_cpus + EXTRA>
{};

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


class Per_cpu_data_up
{
public:
  static void init_ctors() {}
  static void run_ctors(Cpu_number)
  {
    extern ctor_function_t __PER_CPU_INIT_ARRAY_START__[];
    extern ctor_function_t __PER_CPU_INIT_ARRAY_END__[];
    run_ctor_functions(__PER_CPU_INIT_ARRAY_START__, __PER_CPU_INIT_ARRAY_END__);

    extern ctor_function_t __PER_CPU_CTORS_LIST__[];
    extern ctor_function_t __PER_CPU_CTORS_END__[];
    run_ctor_functions(__PER_CPU_CTORS_LIST__, __PER_CPU_CTORS_END__);
  }

  static void run_late_ctors(Cpu_number)
  {
    extern ctor_function_t __PER_CPU_LATE_INIT_ARRAY_START__[];
    extern ctor_function_t __PER_CPU_LATE_INIT_ARRAY_END__[];
    run_ctor_functions(__PER_CPU_LATE_INIT_ARRAY_START__,
                       __PER_CPU_LATE_INIT_ARRAY_END__);

    extern ctor_function_t __PER_CPU_LATE_CTORS_LIST__[];
    extern ctor_function_t __PER_CPU_LATE_CTORS_END__[];
    run_ctor_functions(__PER_CPU_LATE_CTORS_LIST__, __PER_CPU_LATE_CTORS_END__);
  }

  static bool valid(Cpu_number cpu) noexcept
  {
#if defined NDEBUG
    (void)cpu;
    return 1;
#else
    return cpu == Cpu_number::boot_cpu();
#endif
  }

  enum With_cpu_num { Cpu_num };

protected:
  template<typename T>
  static T per_cpu_ref(T adr, Cpu_number)
  {
    return adr;
  }

  template<typename T>
  static void add_ctor_wo_arg(T *) noexcept
  {}

  template<typename T>
  static void add_ctor_w_arg(T *) noexcept
  {}
};


#if defined(CONFIG_MP)

using Per_cpu_data = Per_cpu_data_mp;

#define DEFINE_PER_CPU_CTOR_UID(b) __per_cpu_ctor_ ## b
#define DEFINE_PER_CPU_CTOR_DATA(id) \
  __attribute__((section(".bss.per_cpu_ctor_data"),used)) \
    static Per_cpu_ctor_data DEFINE_PER_CPU_CTOR_UID(id);

#else // !MP

using Per_cpu_data = Per_cpu_data_up;

#define DEFINE_PER_CPU_CTOR_DATA(id)

#endif // !MP

#define DEFINE_PER_CPU_P(p) \
  DEFINE_PER_CPU_CTOR_DATA(__COUNTER__) \
  __attribute__((section(".per_cpu.data"),init_priority(0xfffe - p)))

#define DEFINE_PER_CPU      DEFINE_PER_CPU_P(9)
#define DEFINE_PER_CPU_LATE DEFINE_PER_CPU_P(19)

template< typename T > class Per_cpu_ptr;

template< typename T >
class Per_cpu : private Per_cpu_data
{
  friend class Per_cpu_ptr<T>;
public:
  typedef T Type;

  Per_cpu() noexcept
  {
    add_ctor_wo_arg(&_d);
  }

  explicit Per_cpu(With_cpu_num) noexcept
  : _d(Cpu_number::boot_cpu())
  {
    add_ctor_w_arg(&_d);
  }


  template<typename TEST>
  Cpu_number find_cpu(TEST const &test) const
  {
    for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
      if (valid(i) && test(cpu(i)))
        return i;

    return Cpu_number::nil();
  }

  T const &cpu(Cpu_number cpu) const noexcept
  { return *per_cpu_ref(&_d, cpu); }

  T &cpu(Cpu_number cpu) noexcept
  { return *per_cpu_ref(&_d, cpu); }

  T const &current() const noexcept { return cpu(current_cpu()); }
  T &current() noexcept { return cpu(current_cpu()); }

private:
  T _d;

};

template< typename T >
class Per_cpu_ptr : private Per_cpu_data
{
public:
  typedef typename cxx::conditional<
    cxx::is_const<T>::value,
    Per_cpu<typename cxx::remove_cv<T>::type> const,
    Per_cpu<typename cxx::remove_cv<T>::type> >::type Per_cpu_type;

  Per_cpu_ptr() = default;
  Per_cpu_ptr(Per_cpu_type *o) noexcept
  : _p(&o->_d)
  {}

  Per_cpu_ptr &operator = (Per_cpu_type *o) noexcept
  {
    _p = &o->_d;
    return *this;
  }

  T &cpu(Cpu_number cpu) const noexcept
  {
    return *per_cpu_ref(_p, cpu);
  }

  T &current() const noexcept
  {
    return cpu(current_cpu());
  }

private:
  T *_p;
};

