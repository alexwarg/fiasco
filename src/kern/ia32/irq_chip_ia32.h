#pragma once

#include <globals.h>
#include <idt_init.h>
#include <irq_chip.h>
#include <boot_alloc.h>
#include <spin_lock.h>
#include <lock_guard.h>
#include <cassert>

/**
 * Allocator for IA32 interrupt vectors in the IDT.
 *
 * Some vectors are fixed purpose, others can be dynamically
 * managed by this allocator to support MSIs and multiple IO-APICs.
 */
class Int_vector_allocator
{
public:
  enum
  {
    /// Start at vector 0x20, note: <0x10 is vorbidden here
    Base = 0x20,

    /// The Last vector + 1 that is managed
    End  = APIC_IRQ_BASE - 0x08
  };

  bool empty() const { return !_first; }

  void add_free(unsigned start, unsigned end);

  void free(unsigned v)
  {
    assert (Base <= v && v < End);

    auto g = lock_guard(_lock);
    _vectors[v - Base] = _first;
    _first = v;
  }

  unsigned alloc()
  {
    if (!_first)
      return 0;

    auto g = lock_guard(_lock);
    unsigned r = _first;
    if (!r)
      return 0;

    _first = _vectors[r - Base];
    return r;
  }

private:
  /// array for free list
  unsigned char _vectors[End - Base];

  /// the first free vector
  unsigned _first;

  Spin_lock<> _lock;
};

/**
 * Generic IA32 IRQ chip class.
 *
 * Uses an array of Idt_entry_code objects to manage
 * the IRQ entry points and the Irq_base objects assigned to the
 * pins of a specific controller.
 */
class Irq_chip_ia32
{
public:
  /**
   * \note This code is not thread / MP safe.
   */
  explicit Irq_chip_ia32(unsigned irqs);

  /// Number of pins at this chip.
  unsigned nr_irqs() const { return _irqs; }

  Irq_base *irq(Mword irqn) const;
  bool reserve(Mword irqn);

protected:
  unsigned _irqs;
  unsigned char *_vec;
  Spin_lock<> _entry_lock;

  static Int_vector_allocator _vectors;

  unsigned char vector(Mword pin) const
  { return _vec[pin]; }

  /**
   * \pre `irq->irqLock()` must be held
   */
  template<typename CHIP>
  unsigned valloc(Irq_base *irq, Mword pin, unsigned vector, bool init)
  {
    auto g = lock_guard(_entry_lock);
    unsigned v = _valloc(pin, vector);
    if (!v)
      return 0;

    static_cast<CHIP*>(this)->bind(irq, pin, !init);
    _vsetup(irq, pin, v);
    return v;
  }

  bool vfree(Irq_base *irq, void *handler);

private:
  unsigned _valloc(Mword pin, unsigned vector);
  unsigned _vsetup(Irq_base *irq, Mword pin, unsigned vector);

};

