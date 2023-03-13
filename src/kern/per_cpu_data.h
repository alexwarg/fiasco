#pragma once

#include "types.h"
#include "static_init.h"
#include "config.h"
#include "context_base.h"
#include "per_cpu_array.h"

#include <cxx/type_traits>

#include <cstddef>

#if defined(CONFIG_MP)
#include "per_cpu_data_mp.h"
#else // !MP
#include "per_cpu_data_up.h"
#endif // !MP

#define DEFINE_PER_CPU_P(p) \
  DEFINE_PER_CPU_CTOR_DATA(__COUNTER__) \
  __attribute__((section(".per_cpu.data"),init_priority(0xfffe - p)))

#define DEFINE_PER_CPU      DEFINE_PER_CPU_P(9)
#define DEFINE_PER_CPU_LATE DEFINE_PER_CPU_P(19)

template< typename T > class Per_cpu_ptr;

struct Per_cpu_data_all_cpus
{
  struct Iter
  {
    Cpu_number n;

    void inc() noexcept
    {
      while (EXPECT_FALSE(!Per_cpu_data::valid(n) && n < Config::max_num_cpus()))
        ++n;
    }

    explicit Iter(Cpu_number n) noexcept : n(n)
    { inc(); }

    Iter() = default;

    Cpu_number operator * () const noexcept { return n; }

    Iter &operator ++ () noexcept
    {
      ++n;
      inc();
      return *this;
    }

    bool operator != (Iter const &o) const noexcept
    { return n != o.n; }
  };

  Iter begin() const noexcept { return Iter(Cpu_number::first()); }
  Iter end() const noexcept { return Iter(Config::max_num_cpus()); }
};

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
    for (auto i: all())
      if (test(cpu(i)))
        return i;

    return Cpu_number::nil();
  }

  T const &cpu(Cpu_number cpu) const noexcept
  { return *per_cpu_ref(&_d, cpu); }

  T &cpu(Cpu_number cpu) noexcept
  { return *per_cpu_ref(&_d, cpu); }

  T const &current() const noexcept { return cpu(current_cpu()); }
  T &current() noexcept { return cpu(current_cpu()); }

  static Per_cpu_data_all_cpus all() noexcept
  {
    return Per_cpu_data_all_cpus();
  }

  struct Iter : public Per_cpu_data_all_cpus::Iter
  {
    Per_cpu &v;

    Iter(Per_cpu &v, Cpu_number n) : Per_cpu_data_all_cpus::Iter(n), v(v) {}

    T &operator * () const noexcept { return v.cpu(n); }
  };

  Iter begin() const noexcept { return Iter(Cpu_number::first()); }
  Iter end() const noexcept { return Iter(Config::max_num_cpus()); }

private:
  T _d;

};

template< typename T >
class Per_cpu_ptr : private Per_cpu_data
{
public:
  typedef typename cxx::conditional<
    cxx::is_const<T>::value,
    Per_cpu<cxx::remove_cv_t<T>> const,
    Per_cpu<cxx::remove_cv_t<T>> >::type Per_cpu_type;

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

