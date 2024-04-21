#pragma once

#include <types.h>
#include <cxx/bitfield>

#include <cassert>

#include <globalconfig.h>

/**
 * Pseudo descriptor pointing to a GDT/IDT.
 */
class Pseudo_descriptor
{
public:

  Pseudo_descriptor() = default;

  Pseudo_descriptor(Address base, Unsigned16 limit)
  : _limit(limit), _base(base)
  {}

  Address base() const
  {
    return _base;
  }

  Unsigned16 limit() const
  {
    return _limit;
  }

private:
  Unsigned16 _limit = 0;
  Address _base = 0;
} __attribute__((packed));

/**
 * Generic constants for x86 segment descriptors.
 */
class X86desc
{
protected:
  Unsigned64 _value = 0;

  explicit X86desc(Unsigned64 v) : _value(v) {}

public:
  X86desc() = default;
  /**
   * System segment descriptor and gate descriptor types.
   */
  enum Type_system : Unsigned8
  {
    Ldt           = 0x02,
    Task_gate     = 0x05,
    Tss_available = 0x09,
    Tss_busy      = 0x0b,
    Intr_gate     = 0x0e,
    Trap_gate     = 0x0f
  };

  /**
   * Descriptor privilege level bits.
   */
  enum Dpl : Unsigned8
  {
    Kernel = 0x00,
    User   = 0x03
  };

  // system descriptor ?
  bool system() const
  {
    return !_not_system();
  }

  Unsigned64 raw_value() const
  { return _value; }

  CXX_BITFIELD_MEMBER(32 + 8, 32 + 11, type_system, _value);
  CXX_BITFIELD_MEMBER(32 + 12, 32 + 12, _not_system, _value);
  CXX_BITFIELD_MEMBER(32 + 13, 32 + 14, dpl, _value);
  CXX_BITFIELD_MEMBER(32 + 15, 32 + 15, present, _value);
};

/**
 * Global Descriptor Table segment descriptor.
 *
 * \note The value of the \ref _not_system bit determines whether the
 *       \ref _accessed and \ref _type bits or the \ref _type_system bits
 *       are valid.
 */
class Gdt_entry : public X86desc
{
public:
  /**
   * Type bits (code/data, expand-down/conforming, write/read) in non-system
   * segment descriptor.
   */
  enum Type : Unsigned8
  {
    Data_write = 0x01,
    Code_read  = 0x05
  };

  /**
   * Accessed bit in segment descriptor.
   */
  enum Access : Unsigned8
  {
    Not_accessed = 0x00,
    Accessed     = 0x01
  };

  /**
   * 64-bit code segment (L) bit in segment descriptor.
   */
  enum Code : Unsigned8
  {
    Code_undef  = 0x00,
    Code_compat = 0x00,
    Code_64bit  = 0x01
  };

  /**
   * Default operation size bits in segment descriptor.
   */
  enum Default_size : Unsigned8
  {
    Size_undef = 0x00,
    Size_16    = 0x00,
    Size_32    = 0x01
  };

  /**
   * Limit granularity bit in segment descriptor.
   */
  enum Granularity : Unsigned8
  {
    Granularity_bytes = 0x00,
    Granularity_4k    = 0x01
  };


  /**
   * Create a non-present segment descriptor.
   */
  Gdt_entry() = default;

#ifdef CONFIG_BIT64
  /**
   * Create the second half of a system segment descriptor.
   *
   * \param base  Segment base address.
   */
  explicit Gdt_entry(Address base)
  {
    _value = base >> 32;
  }
#endif

  /**
   * Create a system segment descriptor.
   *
   * \note On AMD64, the second half of the system segment descriptor has to be
   *       created separately.
   *
   * \param base         Segment base address.
   * \param limit        Segment limit (already pre-shifted in case of 4K
   *                     granularity).
   * \param type_system  Type of the system segment descriptor.
   * \param dpl          Descriptor privilege level.
   * \param granularity  Limit granularity.
   */
  Gdt_entry(Address base, Unsigned32 limit,
            Type_system type_system,
            Dpl dpl, Granularity granularity)
  : X86desc(
        _limit_low_bfm_t::val(limit & 0x0000ffffU)
      | _base_low_bfm_t::val(base & 0x00ffffffU)
      | type_system_bfm_t::val(type_system)
      | _not_system_bfm_t::val(0)
      | dpl_bfm_t::val(dpl)
      | present_bfm_t::val(1)
      | _limit_high_bfm_t::val((limit & 0x00f0000U) >> 16)
      | available_bfm_t::val(0)
      | code_bfm_t::val(0)
      | default_size_bfm_t::val(0)
      | granularity_bfm_t::val(granularity)
      | _base_high_bfm_t::val((base & 0xff000000U) >> 24)
      )
  {}

  /**
   * Create a non-system segment descriptor.
   *
   * \param base          Segment base address.
   * \param limit         Segment limit (already pre-shifted in case of 4K
   *                      granularity).
   * \param accessed      Accessed bit.
   * \param type          Segment type.
   * \param code          64-bit code segment flag (should be set to Code_undef
   *                      in case of data segments and for IA-32).
   * \param default_size  Default operation size (should be set to Size_undef
   *                      in case of 64-bit code segments).
   * \param granularity   Limit granularity.
   */
  Gdt_entry(Address base, Unsigned32 limit,
            Access accessed, Type type,
            Dpl dpl, Code code,
            Default_size default_size,
            Granularity granularity)
  : X86desc(
        _limit_low_bfm_t::val(limit & 0x0000ffffU)
      | _base_low_bfm_t::val(base & 0x00ffffffU)
      | accessed_bfm_t::val(accessed)
      | type_bfm_t::val(type)
      | _not_system_bfm_t::val(1)
      | dpl_bfm_t::val(dpl)
      | present_bfm_t::val(1)
      | _limit_high_bfm_t::val((limit & 0x00f0000U) >> 16)
      | available_bfm_t::val(0)
      | code_bfm_t::val(code)
      | default_size_bfm_t::val(default_size)
      | granularity_bfm_t::val(granularity)
      | _base_high_bfm_t::val((base & 0xff000000U) >> 24)
      )
  {
    assert(IS_ENABLED(CONFIG_BIT64) || code == Code_undef);
    assert(type == Code_read || code == Code_undef);
    assert(code != Code_64bit || default_size == Size_undef);
  }

  // non-system segment is writeable ?
  bool writable() const
  {
    return type() & 0x02;
  }

  Unsigned32 limit() const
  {
    return static_cast<Unsigned32>(_limit_low())
           | (static_cast<Unsigned32>(_limit_high()) << 16);
  }

  // size in bytes
  Mword size() const
  {
    Mword value = limit();

    if (granularity() == Granularity_4k)
      return ((value + 1) << 12) - 1;

    return value;
  }

  /**
   * Get whether the segment descriptor is unsafe for user access.
   *
   * A segment descriptor is unsafe if its privilege level is not user space or
   * if it is a system descriptor.
   *
   * \retval True if segment descriptor is unsafe for user access.
   * \retval False if segment descriptor is safe for user access.
   */
  bool unsafe() const
  {
    return present() && ((dpl() != User) || system());
  }

  void tss_make_available()
  {
    assert(system());
    assert(type_system() == Tss_available || type_system() == Tss_busy);

    type_system() = Tss_available;
  }

  Address base() const
  {
    Address base = static_cast<Address>(_base_low())
                 | (static_cast<Address>(_base_high()) << 24);

   if (IS_ENABLED(CONFIG_BIT32) || _not_system())
    return base;

   /*
    * Long mode system segment descriptor occupies two 64-bit slots and the
    * upper 32 bits of the base address follow in the lower 32 bits of the next
    * 64-bit slot.
    */

   return base | (this[1]._value << 32);
  }

  // Get descriptor size (in bytes).
  unsigned long desc_size() const
  {
    // In 32-bit mode, all descriptors are 64 bit.
    if (sizeof(long) == sizeof(int))
      return 8;

    // Non-system descriptors are 64 bit.
    if (!system())
      return 8;

    // Non-reserved system descriptors (in 64-bit mode) are 128 bit.
    switch (type_system())
      {
      case Ldt:
      case Tss_available:
      case Tss_busy:
      case Intr_gate:
      case Trap_gate:
        return 16;
      default:
        break;
      }

    // Reserved system descriptors are still 64 bit.
    return 8;
  }

  CXX_BITFIELD_MEMBER(32 +  8, 32 +  8, accessed, _value);
  CXX_BITFIELD_MEMBER(32 +  9, 32 + 11, type, _value);
  CXX_BITFIELD_MEMBER(32 + 20, 32 + 20, available, _value);
  CXX_BITFIELD_MEMBER(32 + 21, 32 + 21, code, _value);
  CXX_BITFIELD_MEMBER(32 + 22, 32 + 22, default_size, _value);
  CXX_BITFIELD_MEMBER(32 + 23, 32 + 23, granularity, _value);

private:
  CXX_BITFIELD_MEMBER(0, 15, _limit_low, _value);
  CXX_BITFIELD_MEMBER(16, 32 + 7, _base_low, _value);

  CXX_BITFIELD_MEMBER(32 + 16, 32 + 19, _limit_high, _value);
  CXX_BITFIELD_MEMBER(32 + 24, 32 + 31, _base_high, _value);
};

/**
 * Interrupt Descriptor Table gate descriptor.
 */
class Idt_entry_32 : public X86desc
{
public:
  Idt_entry_32() = default;

  /**
   * Create an interrupt/trap gate descriptor.
   *
   * \param offset       Offset.
   * \param selector     Target selector.
   * \param type_system  Descriptor type (Intr_gate or Trap_gate).
   * \param dpl          Descriptor privilege level.
   */
  Idt_entry_32(Address offset, Unsigned16 selector,
               Type_system type_system, Dpl dpl,
               Unsigned8 ist = 0)
  : X86desc(
        _offset_low_bfm_t::val(offset & 0x0000ffffU)
      | selector_bfm_t::val(selector)
      | _ist_bfm_t::val(ist)
      | type_system_bfm_t::val(type_system)
      | dpl_bfm_t::val(dpl)
      | present_bfm_t::val(1)
      | _offset_med_bfm_t::val((offset & 0xffff0000U) >> 16)
      )
  {
    assert(type_system == Intr_gate || type_system == Trap_gate);
  }

  /**
   * Create a task gate descriptor.
   *
   * \param selector  Target selector.
   * \param dpl       Descriptor privilege level.
   */
  Idt_entry_32(Unsigned16 selector, Dpl dpl)
  : X86desc(
          selector_bfm_t::val(selector)
        | type_system_bfm_t::val(Task_gate)
        | dpl_bfm_t::val(dpl)
        | present_bfm_t::val(1)
      )
  {}

  Address offset() const
  {
    return static_cast<Address>(_offset_low())
           | (static_cast<Address>(_offset_med()) << 16);
  }

  CXX_BITFIELD_MEMBER(16, 31, selector, _value);

protected:
  CXX_BITFIELD_MEMBER(0, 15, _offset_low, _value);
  CXX_BITFIELD_MEMBER(32 + 0, 32 + 2, _ist, _value);
  CXX_BITFIELD_MEMBER(32 + 3, 32 + 7, _zeroes, _value);
  CXX_BITFIELD_MEMBER(32 + 16, 32 + 31, _offset_med, _value);
};

#ifdef CONFIG_BIT32
using Idt_entry = Idt_entry_32;
#endif // CONFIG_BIT32
#ifdef CONFIG_BIT64

class Idt_entry : public Idt_entry_32
{
public:
  Idt_entry() = default;

  /**
   * Create an interrupt/trap gate descriptor.
   *
   * \param offset       Offset.
   * \param selector     Target selector.
   * \param type_system  Descriptor type (Intr_gate or Trap_gate).
   * \param dpl          Descriptor privilege level.
   * \param ist          Interrupt stack table.
   */
  Idt_entry(Address offset, Unsigned16 selector,
            Type_system type_system, Dpl dpl,
            Unsigned8 ist = 0)
  : Idt_entry_32(offset, selector, type_system, dpl, ist),
    _value_high(_offset_high_bfm_t::val((offset & 0xffffffff00000000ULL) >> 32))
  {
    assert(type_system == Intr_gate || type_system == Trap_gate);
  }

  Address offset() const
  {
    return static_cast<Address>(_offset_low())
           | (static_cast<Address>(_offset_med()) << 16)
           | (static_cast<Address>(_offset_high()) << 32);
  }

  // Task gate not supported on IA32e
  Idt_entry(Unsigned16, Dpl)
  { assert(false /* no task gate on amd64 */); }

private:
  Unsigned64 _value_high = 0;

  CXX_BITFIELD_MEMBER(0, 31, _offset_high, _value_high);
  CXX_BITFIELD_MEMBER(32, 63, _reserved, _value_high);
};

#endif // CONFIG_BIT64

