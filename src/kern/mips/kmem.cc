
#include <kmem.h>
#include <cassert>

Pdir *const Kmem::kdir = 0;

Address
Kmem::mmio_remap(Address phys, Address size)
{
  (void)size;
  assert ((phys + size < 0x20000000) && "MMIO outside KSEG1");
  return phys + KSEG1;
}
