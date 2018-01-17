#pragma once

#include <mem_space_base.h>
#include <id_alloc.h>
#include <alternatives.h>
#include <mem_unit.h>
#include <vz.h>


class Mem_space_vz
{
public:
  template<typename M = Mem_space>
  unsigned switchin_guest_context()
  {
    auto c = current_cpu();
    unsigned guest_id = _guest_id_alloc.cpu(c).alloc(this, c);

    asm volatile (ALTERNATIVE_INSN(
          "nop",
          "mtc0 %0, $10, 4",  // Load GuestCtl1 with guest ID
          0x4 /* FEATURE_VZ */)
        : : "r"(guest_id));

    Mem_unit::set_current_asid(0);
    M::_current.current() = static_cast<M *>(this);
    // no ehb here as we use the mappings after the eret only
    return guest_id;
  }

protected:
  bool _is_vz_guest = false;

  typedef Per_cpu_array<unsigned char> Guest_id_array;
  Guest_id_array _guest_id;

  struct Guest_id_ops
  {
    enum { Id_offset = 1 };

    static bool valid(Mem_space_vz *o, Cpu_number cpu)
    { return o->_guest_id[cpu] != 0; }

    static unsigned get_id(Mem_space_vz *o, Cpu_number cpu)
    { return o->_guest_id[cpu]; }

    static bool can_replace(Mem_space_vz *v, Cpu_number cpu)
    { return v->_guest_id[cpu] != Vz::owner.cpu(cpu).guest_id; }

    static void set_id(Mem_space_vz *o, Cpu_number cpu, int id)
    {
      write_now(&o->_guest_id[cpu], (unsigned char)id);
      Mem_unit::tlb_flush(-1, id);
      Mem_unit::vz_guest_tlb_flush(id);
    }

    static void reset_id(Mem_space_vz *o, Cpu_number cpu)
    { write_now(&o->_guest_id[cpu], (unsigned char)0); }
  };

  struct Guest_id_alloc : Id_alloc<unsigned char, Mem_space_vz, Guest_id_ops>
  {
    static unsigned n_guest_ids(Cpu_number cpu)
    {
      if (!Cpu::cpus.cpu(cpu).options.vz())
        return 0;

      Mword x;
      asm volatile (
          ".set push \n"
          ".set noat \n"
          "mfc0 $1, $10, 4 \n"
          "move %0, $1 \n"
          "ori %0, 0xff \n"
          "mtc0 %0, $10, 4 \n"
          "ehb \n"
          "mfc0 %0, $10, 4 \n"
          "mtc0 $1, $10, 4 \n"
          "ehb \n" // overly paranoid ehb here, usually the guest ID is first
                   // used when accessing mapped memory, or by the GIC
          ".set pop"
          : "=r"(x));

      return (x & 0xff);
    }

    Guest_id_alloc(Cpu_number cpu)
    : Id_alloc<unsigned char, Mem_space_vz, Guest_id_ops>(n_guest_ids(cpu))
    {}
  };

  static Per_cpu<Guest_id_alloc> _guest_id_alloc;

  void apply_extra_page_attribs(Mem_space_base::Attr *a)
  {
    // ATTENTION: Setting the global bit for VM page-tables prevents us
    // from running normal threads in this mem-space, hence
    // caps & Caps::threads() must be false.
    if (_is_vz_guest)
      a->kern |= Page::Kern::Global();
  }

  void set_guest_ctl1_rid(bool guest)
  {
    unsigned long gid = 0;
    if (guest)
        gid = _guest_id[current_cpu()];
    asm volatile (ALTERNATIVE_INSN(
          "nop",
          ".set push; .set noat; mfc0 $1, $10, 4; ins $1, %0, 16, 8; mtc0 $1, $10, 4; .set pop",
          0x4 /* FEATURE_VZ */)
        : : "r"(gid));
  }


protected:
  void guest_id_init()
  {
    for (Guest_id_array::iterator i = _guest_id.begin(); i != _guest_id.end(); ++i)
      *i = 0;
  }

  void reset_guest_id()
  {
    for (auto i: _guest_id_alloc.all())
      _guest_id_alloc.cpu(i).free(this, i);
  }
};

