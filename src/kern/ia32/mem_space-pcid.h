#pragma once

#include <id_alloc.h>
#include <types.h>
#include <mem_unit.h>
#include <per_cpu_data.h>

class Mem_space_ia32_pcid_base
{
public:
  enum
  {
    Asid_num = (1 << 12) - 1,
    Asid_base = 1
  };

  Mem_space_ia32_pcid_base()
  {
    asid(Mem_unit::Asid_invalid);
  }

protected:
  void reset_asid()
  {
    for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
      _asid_alloc.cpu(i).free(this, i);
  }

  void asid(unsigned long a)
  {
    for (Asid_array::iterator i = _asid.begin(); i != _asid.end(); ++i)
      *i = a;
  }

  unsigned long c_asid() const
  { return _asid[current_cpu()]; }

  void tlb_flush_this_()
  {
    auto asid = c_asid();
    if (asid != Mem_unit::Asid_invalid)
      Mem_unit::tlb_flush(asid);
  }

protected:
  typedef Per_cpu_array<unsigned long> Asid_array;
  Asid_array _asid;

  struct Asid_ops
  {
    enum { Id_offset = Asid_base };

    static bool valid(Mem_space_ia32_pcid_base *o, Cpu_number cpu)
    { return o->_asid[cpu] != Mem_unit::Asid_invalid; }

    static unsigned long get_id(Mem_space_ia32_pcid_base *o, Cpu_number cpu)
    { return o->_asid[cpu]; }

    static void set_id(Mem_space_ia32_pcid_base *o, Cpu_number cpu, unsigned long id)
    {
      write_now(&o->_asid[cpu], id);
      Mem_unit::tlb_flush(id);
    }

    static void reset_id(Mem_space_ia32_pcid_base *o, Cpu_number cpu)
    { write_now(&o->_asid[cpu], (unsigned long)Mem_unit::Asid_invalid); }
  };


  struct Asid_alloc : Id_alloc<Unsigned16, Mem_space_ia32_pcid_base, Asid_ops>
  {
    Asid_alloc() : Id_alloc<Unsigned16, Mem_space_ia32_pcid_base, Asid_ops>(Asid_num) {}
  };

  static Per_cpu<Asid_alloc> _asid_alloc;
};



template<typename M>
class Mem_space_ia32_pcid : public Mem_space_ia32_pcid_base
{
  struct Asid_ops : Mem_space_ia32_pcid_base::Asid_ops
  {
    static bool can_replace(Mem_space_ia32_pcid_base *v, Cpu_number cpu)
    { return v != M::current_mem_space(cpu); }
  };

public:
  unsigned long asid()
  {
    Cpu_number cpu = current_cpu();
    return _asid_alloc.cpu(cpu).alloc<Asid_ops>(this, cpu);
  }

};
