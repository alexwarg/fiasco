#include <irq_chip_ia32.h>

#include <cassert>
#include <cpu_lock.h>
#include <idt.h>
#include <mem.h>
#include <irq_entry_stub.h>

// The global INT vector allocator for IRQs uses these data
Int_vector_allocator Irq_chip_ia32::_vectors;


/**
 * Add free vectors to the allocator.
 * \note This code is not thread / MP safe and assumed to be executed at boot
 * time.
 */
void
Int_vector_allocator::add_free(unsigned start, unsigned end)
{
  assert (Base > 0x10);
  assert (End > Base);
  assert (start >= Base);
  assert (end <= End);

  for (unsigned v = start - Base; v < end - Base - 1; ++v)
    _vectors[v] = v + Base + 1;

  _vectors[end - Base - 1] = _first;
  _first = start;
}

/**
 * \note This code is not thread / MP safe.
 */
Irq_chip_ia32::Irq_chip_ia32(unsigned irqs)
: _irqs(irqs),
  _vec(irqs ? (unsigned char *)Boot_alloced::alloc(irqs) : 0),
  _entry_lock(Spin_lock<>::Unlocked)
{
  for (unsigned i = 0; i < irqs; ++i)
    _vec[i] = 0;

  // add vectors from 0x40 up to Int_vector_allocator::End
  // as free if we are the first IA32 chip ctor running
  if (_vectors.empty())
    _vectors.add_free(0x34, Int_vector_allocator::End);
}


Irq_base *
Irq_chip_ia32::irq(Mword irqn) const
{
  if (irqn >= _irqs)
    return nullptr;

  if (!_vec[irqn])
    return nullptr;

  extern Irq_entry_stub idt_irq_vector_stubs[];
  return idt_irq_vector_stubs[_vec[irqn] - 0x20].irq;
}

/**
 * Generic binding of an Irq_base object to a specific pin and a 
 * an INT vector.
 *
 * \param irq The Irq_base object to bind
 * \param pin The pin number at this IRQ chip
 * \param vector The INT vector to use, or 0 for dynamic allocation
 * \return the INT vector used an success, or 0 on failure.
 *
 * This function does the following:
 * 1. Some sanity checks
 * 2. Check if PIN is unassigned
 * 3. Check if no vector is given:
 *    a) Use vector that was formerly assigned to this PIN
 *    b) Try to allocate a new vector for the PIN
 * 4. Prepare the entry code to point to \a irq
 * 5. Point IDT entry to the PIN's entry code
 * 6. Return the assigned vector number
 */
unsigned
Irq_chip_ia32::_valloc(Mword pin, unsigned vector)
{
  if (pin >= _irqs)
    return 0;

  if (vector >= Int_vector_allocator::End)
    return 0;

  if (_vec[pin])
    return 0;

  if (!vector)
    vector = _vectors.alloc();

  return vector;
}

unsigned
Irq_chip_ia32::_vsetup(Irq_base *irq, Mword pin, unsigned vector)
{
  _vec[pin] = vector;
  extern Irq_entry_stub idt_irq_vector_stubs[];
  auto p = idt_irq_vector_stubs + vector - 0x20;
  p->irq = irq;

  // force code to memory before setting IDT entry
  Mem::barrier();

  Idt::set_entry(vector, (Address)p, false);
  return vector;
}

/**
 * \pre `irq->irqLock()` must be held
 */
bool
Irq_chip_ia32::vfree(Irq_base *irq, void *handler)
{
  extern Irq_entry_stub idt_irq_vector_stubs[];
  unsigned v = _vec[irq->pin()];
  assert (v);
  assert (idt_irq_vector_stubs[v - 0x20].irq == irq);

  Idt::set_entry(v, (Address)handler, false);
  idt_irq_vector_stubs[v - 0x20].irq = 0;
  _vec[irq->pin()] = 0;

  _vectors.free(v);
  return true;
}


bool
Irq_chip_ia32::reserve(Mword irqn)
{
  if (irqn >= _irqs)
    return false;

  if (_vec[irqn])
    return false;

  _vec[irqn] = 0xff;
  return true;
}
