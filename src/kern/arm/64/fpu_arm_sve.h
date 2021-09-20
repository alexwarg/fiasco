#pragma once

#include <mem.h>
#include <cpu.h>
#include <fpu_state_ptr.h>

#include <globalconfig.h>

#include <cxx/type_traits>

enum class Fpu_state_type
{
  None,
  Simd,
  Sve,

  Default_state_type = Simd
};

class Fpu_state
{
public:
  virtual Fpu_state_type type() const = 0;
  virtual void init() = 0;
  virtual void save() = 0;
  virtual void restore() const = 0;
  virtual void copy(Fpu_state const *from) = 0;
};

class Fpu_arch_base
{
public:
  using State_type = Fpu_state_type;
  static constexpr State_type Default_state_type = State_type::Simd;
  static unsigned state_size(State_type type = Default_state_type);
  static void init(Cpu_number, bool);
  static void init_state(Fpu_state *fpu_state, State_type type);

private:
  /// SVE support detected?
  static bool _has_sve;
  /// Vector length in quad-words (128-bits)
  static unsigned _max_vl;
};


