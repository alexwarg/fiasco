#pragma once

#include <initcalls.h>
#include <per_cpu_data.h>
#include <types.h>
#include <globalconfig.h>
#include <cstring>

#ifdef CONFIG_FPU

#include <fpu_arch.h>

#else // CONFIG_FPU

struct Fpu_state;

struct Fpu_arch
{
  static void init_state(Fpu_state *)
  {}

  static unsigned state_size()
  { return 0; }

  static unsigned state_align()
  { return 1; }

  static void init(Cpu_number, bool)
  {}

  static void save_state(Fpu_state *)
  {}

  static void restore_state(Fpu_state const *)
  {}

  static void copy_state(Fpu_state *, Fpu_state const *)
  {}

  static void disable()
  {}

  static void enable() {}
};

#endif // CONFIG_FPU

class Context;
class Fpu_state;
class Trap_state;

class Fpu : public Fpu_arch
{
public:
  static Per_cpu<Fpu> fpu;

#ifdef CONFIG_FPU
#ifdef CONFIG_LAZY_FPU
public:
  Context *owner() const { return _owner; }
  void set_owner(Context *owner) { _owner = owner; }
  bool is_owner(Context *owner) const { return _owner == owner; }

  static void restore_state(Fpu_state const *s)
  { Fpu_arch::restore_state(s, Fpu::fpu.current().owner()); }

private:
  Context *_owner;

#else // CONFIG_LAZY_FPU
public:
  static void restore_state(Fpu_state const *s)
  { Fpu_arch::restore_state(s, true); }

#endif // CONFIG_LAZY_FPU
#else // CONFIG_FPU
#endif // CONFIG_FPU
};

