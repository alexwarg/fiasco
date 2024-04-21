#pragma once

#include <globalconfig.h>

#ifdef CONFIG_NO_LDT
#include_next <space-arch.h>
#else

#include <cpu.h>

class Space_ia32_ldt_base
{
protected:
  class Ldt
  {
  public:
    Ldt() : _addr(0), _size(0) {}
    Address addr() const { return reinterpret_cast<Address>(_addr); }
    Mword   size() const { return _size; }

    void size(Mword size)
    { _size = size; }

    void alloc();

    ~Ldt();

  private:
    void *_addr;
    Mword _size;
  };

  friend class Jdb_misc_debug;

  template<typename SPACE, typename FLAGS>
  void switchin_context(SPACE *from, FLAGS)
  {
    if (this != from)
      Cpu::cpus.cpu(current_cpu()).enable_ldt(_ldt.addr(), _ldt.size());
  }

  Ldt _ldt;
};

template<typename SPC>
struct Space_arch_mixin : Space_ia32_ldt_base
{};


#endif
