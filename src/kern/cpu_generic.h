#pragma once

#include "cpu_mask.h"
#include "member_offs.h"
#include <globalconfig.h>

class Cpu_generic_base
{
  MEMBER_OFFSET();
public:
  struct By_phys_id
  {
    Cpu_phys_id _p;
    By_phys_id(Cpu_phys_id p) : _p(p) {}
    template<typename CPU>
    bool operator () (CPU const &c) const { return _p == c.phys_id(); }
  };
  // we actually use a mask that has one CPU more that we can physically,
  // have, to avoid lots of special cases for an invalid CPU number
  typedef Cpu_mask_t<Config::Max_num_cpus + 1> Online_cpu_mask;

  enum { Invalid = Config::Max_num_cpus };
  static Cpu_number invalid() { return Cpu_number(Invalid); }

  /** Convenience for Cpu::cpus.cpu(cpu).online() */
  static bool online(Cpu_number cpu);
  /** Is this CPU online ? */
  bool online() const;

  static Online_cpu_mask const &online_mask() { return _online_mask; }
  static Online_cpu_mask const &present_mask() { return _present_mask; }

  static bool is_canonical_address(Address) noexcept
  { return true; }

  struct All_online
  {
    struct Iter
    {
      Cpu_number n;

      void inc()
      {
        while (EXPECT_FALSE(!Cpu_generic_base::online(n) && n < Config::max_num_cpus()))
          ++n;
      }

      explicit Iter(Cpu_number n) : n(n)
      { inc(); }

      Iter() = default;

      Cpu_number operator * () const { return n; }

      Iter &operator ++ ()
      {
        ++n;
        inc();
        return *this;
      }

      bool operator != (Iter const &o) const
      { return n != o.n; }
    };

    Iter begin() { return Iter(Cpu_number::first()); }
    Iter end() { return Iter(Config::max_num_cpus()); }
  };

  static All_online all_online() { return All_online(); }

protected:
  static Online_cpu_mask _online_mask;
  static Online_cpu_mask _present_mask;
};

class Cpu_generic_up : public Cpu_generic_base
{
public:
  Cpu_number id() const noexcept { return Cpu_number::boot_cpu(); }
  bool online() const noexcept { return true; }
  static bool online(Cpu_number _cpu) noexcept { return _cpu == Cpu_number::boot_cpu(); }

protected:
  void set_id(Cpu_number) {}
};

class Cpu_generic_mp : public Cpu_generic_base
{
public:
  Cpu_number id() const noexcept { return _id; }
  bool online() const noexcept { return _online_mask.get(_id); }
  static bool online(Cpu_number _cpu) noexcept { return _online_mask.get(_cpu); }

protected:
  void set_id(Cpu_number id) { _id = id; }
private:
  Cpu_number _id;
};

#ifdef CONFIG_MP
using Cpu_generic_x = Cpu_generic_mp;
#else
using Cpu_generic_x = Cpu_generic_up;
#endif

class Cpu_generic : public Cpu_generic_x
{
public:
  /**
   * Set this CPU to online state.
   * NOTE: This does not activate an inactive CPU, Just set the given state.
   */
  void set_online(bool o) noexcept
  {
    if (o)
      _online_mask.atomic_set(id());
    else
      _online_mask.atomic_clear(id());
  }

  void set_present(bool o) noexcept
  {
    if (o)
      _present_mask.atomic_set(id());
    else
      _present_mask.atomic_clear(id());
  }
};

