
#pragma once

#include "types.h"
#include "mem_space.h"
#include "mem_layout.h"
#include "kmem.h"

#include <cassert>
#include <cxx/atomic>

class Mem_space;
class Space;

class Generic_io_space_base
{
public:
  // We'd rather like to use a "using Mem_space::Status" declaration here,
  // but that wouldn't make the enum values accessible as
  // Generic_io_space::Insert_ok and so on.
  enum Status
  {
    Insert_ok = 0,		///< Mapping was added successfully.
    Insert_warn_exists,		///< Mapping already existed
    Insert_warn_attrib_upgrade,	///< Mapping already existed, attribs upgrade
    Insert_err_nomem,		///< Couldn't alloc new page table
    Insert_err_exists		///< A mapping already exists at the target addr
  };

  typedef void Reap_list;

  typedef Port_number V_pfn;
  typedef Port_number V_pfc;
  typedef Port_number Phys_addr;
  typedef Order Page_order;
  typedef L4_fpage::Rights Attr;

  enum
  {
    Need_insert_tlb_flush = 0,
    Need_upgrade_tlb_flush = 0,
    Need_xcpu_tlb_flush = 0,
    Map_page_size = 1,
    Page_shift = 0,
    Map_superpage_shift = 16,
    Map_superpage_size = 0x10000,
    Map_max_address = 0x10000,
    Whole_space = 16,
    Identity_map = 1,
  };

  struct Fit_size
  {
    Page_order operator () (Page_order o) const
    {
      return o >= Page_order(Map_superpage_shift)
             ? Page_order(Map_superpage_shift)
             : Page_order(0);
    }
  };

  static V_pfn map_max_address()
  { return V_pfn(Map_max_address); }

  static Phys_addr page_address(Phys_addr o, Page_order s)
  { return cxx::mask_lsb(o, s); }

  static Phys_addr subpage_address(Phys_addr addr, V_pfc offset)
  { return addr | offset; }

  static V_pfc subpage_offset(V_pfn addr, Page_order size)
  { return cxx::get_lsb(addr, size); }

  static Mdb_types::Pfn to_pfn(V_pfn p)
  { return Mdb_types::Pfn(cxx::int_value<V_pfn>(p)); }

  static V_pfn to_virt(Mdb_types::Pfn p)
  { return V_pfn(cxx::int_value<Mdb_types::Pfn>(p)); }

  static Mdb_types::Pcnt to_pcnt(Page_order s)
  { return Mdb_types::Pcnt(cxx::int_value<V_pfc>(V_pfc(1) << s)); }

  static Page_order to_order(Mdb_types::Order p)
  { return Page_order(cxx::int_value<Mdb_types::Order>(p)); }

  static V_pfc to_size(Page_order p)
  { return V_pfc(1) << p; }

  static V_pfn canonize(V_pfn v)
  { return v; }

  Fit_size fitting_sizes() const
  {
    return Fit_size();
  }

  static bool is_full_flush(L4_fpage::Rights rights)
  {
    return !rights.empty();
  }

  /** return the IO counter.
   *  @return number of IO ports mapped / 0 if not mapped
   */
  Mword get_io_counter() const noexcept
  {
    return _io_counter & ~0x10000000;
  }

  /** Add something the the IO counter.
      @param incr number to add
      @pre 2nd level page table for IO bitmap is present
  */
  void addto_io_counter(Smword incr) noexcept
  {
    cxx::atomic_fetch_add(&_io_counter, static_cast<Mword>(incr));
  }

protected:
  // DATA
  Mword _io_counter = 0;

  bool is_superpage() const noexcept
  { return _io_counter & 0x10000000; }

  static void free_memory(Mem_space *m, Ram_quota *q);

  static bool io_lookup(Mem_space *m, Address port_number)
  {
    assert(port_number < Mem_layout::Io_port_max);

    // be careful, do not cause page faults here
    // there might be nothing mapped in the IO bitmap

    Address port_addr = get_phys_port_addr(m, port_number);

    if(port_addr == ~0UL)
      return false;		// no bitmap -> no ports

    // so there is memory mapped in the IO bitmap
    char *port = static_cast<char *>(Kmem::phys_to_virt(port_addr));

    // bit == 1 disables the port
    // bit == 0 enables the port
    return !(*port & get_port_bit(port_number));
  }

  Status io_insert(Mem_space *m, Ram_quota *q, Address port_number);

  /** Disable one IO port in the IO space.
      @param port_number port to disable
   */
  void io_delete(Mem_space *m, Address port_number)
  {
    assert(port_number < Mem_layout::Io_port_max);

    // be careful, do not cause page faults here
    // there might be nothing mapped in the IO bitmap

    Address port_addr = get_phys_port_addr(m, port_number);

    if (port_addr == ~0UL)
      // nothing mapped -> nothing to delete
      return;

    // so there is memory mapped in the IO bitmap -> disable the ports
    char *port = static_cast<char *>(Kmem::phys_to_virt(port_addr));

    // bit == 1 disables the port
    // bit == 0 enables the port
    if(!(*port & get_port_bit(port_number)))    // port enabled ??
      {
        *port |= get_port_bit(port_number);
        addto_io_counter(-1);
      }
  }


private:
  static Unsigned8 get_port_bit(Address const port_number)
  {
    return 1 << (port_number & 7);
  }

  static Address get_phys_port_addr(Mem_space *m, Address const port_number)
  {
    return m->virt_to_phys(Mem_layout::Io_bitmap + (port_number >> 3));
  }
};

/** Wrapper class for io_{map,unmap}.  This class serves as an adapter
    for map<Generic_io_space> to Mem_space.
 */
template< typename SPACE >
class Generic_io_space : public Generic_io_space_base
{
  friend class Jdb_iomap;

public:
  static char const * const name;

  Generic_io_space() = default;

  ~Generic_io_space()
  {
    free_memory(mem_space(), ram_quota());
  }

  FIASCO_SPACE_VIRTUAL
  Status v_insert(Phys_addr phys, V_pfn virt, Page_order size, Attr page_attribs)
  FIASCO_FLATTEN
  {
    (void)phys;
    (void)page_attribs;

    assert (phys == virt);
    if (is_superpage() && size == Page_order(Map_superpage_shift))
      return Insert_warn_exists;

    auto m = mem_space();
    auto q = ram_quota();
    if (get_io_counter() == 0 && size == Page_order(Map_superpage_shift))
      {
        for (unsigned p = 0; p < Map_max_address; ++p)
          io_insert(m, q, p);
        _io_counter |= 0x10000000;

        return Insert_ok;
      }

    assert (size == Page_order(0));

    return typename Generic_io_space::Status(io_insert(m, q, cxx::int_value<V_pfn>(virt)));
  }

  FIASCO_SPACE_VIRTUAL
  bool v_lookup(V_pfn virt, Phys_addr *phys = 0, Page_order *order = 0,
                Attr *attribs = 0) FIASCO_FLATTEN
  {
    if (is_superpage())
      {
        if (order) *order = Page_order(Map_superpage_shift);
        if (phys) *phys = Phys_addr(0);
        if (attribs) *attribs = Attr::URW();
        return true;
      }

    if (order) *order = Page_order(0);

    if (io_lookup(cxx::int_value<V_pfn>(virt)))
      {
        if (phys) *phys = virt;
        if (attribs) *attribs = Attr::URW();
        return true;
      }

    if (get_io_counter() == 0)
      {
        if (order) *order = Page_order(Map_superpage_shift);
        if (phys) *phys = Phys_addr(0);
      }

    return false;
  }


  virtual
  bool v_fabricate(V_pfn address, Phys_addr *phys, Page_order *order,
                   Attr *attribs = 0)
  {
    return this->v_lookup(address, phys, order, attribs);
  }

  FIASCO_SPACE_VIRTUAL
  L4_fpage::Rights v_delete(V_pfn virt, Page_order size,
                            L4_fpage::Rights page_attribs)
  FIASCO_FLATTEN
  {
    if (!(page_attribs & L4_fpage::Rights::FULL()))
      return L4_fpage::Rights(0);

    auto m = mem_space();
    if (is_superpage())
      {
        assert (size == Page_order(Map_superpage_shift));

        for (unsigned p = 0; p < Map_max_address; ++p)
          io_delete(m, p);

        _io_counter = 0;
        return L4_fpage::Rights(0);
      }

    (void)size;
    assert (size == Page_order(0));

    io_delete(m, cxx::int_value<V_pfn>(virt));
    return L4_fpage::Rights(0);
  }

  Ram_quota *ram_quota() const noexcept
  {
    return static_cast<SPACE const *>(this)->ram_quota();
  }

  bool io_lookup(Address port_number)
  { return Generic_io_space_base::io_lookup(mem_space(), port_number); }

private:

  Mem_space const *mem_space() const noexcept
  { return static_cast<SPACE const *>(this); }

  Mem_space *mem_space() noexcept
  { return static_cast<SPACE *>(this); }
};

template< typename SPACE>
char const * const Generic_io_space<SPACE>::name = "Io_space";

