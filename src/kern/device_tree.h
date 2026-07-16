#pragma once

#include <stdint.h>
#include <globalconfig.h>

#ifndef CONFIG_DT
#include <device_tree_dummy.h>
#else
#include <device_tree_fdt.h>
#endif

#if 0
IMPLEMENT static
void
Dt::init()
{
  Address fdt_phys = Kip::k()->dt_addr;
  if (!fdt_phys)
    return;

  // Map the first page to determine the device tree size.
  void *fdt = Kmem_mmio::map(fdt_phys, sizeof(struct fdt_header),
                             Kmem_mmio::Map_attr::Cached());
  if (!fdt)
    return;

  int fdt_check = fdt_check_header(fdt);
  if (fdt_check < 0)
    {
      WARN("FDT sanity check failed: %s (%d)\n",
           fdt_strerror(fdt_check), fdt_check);
      Kmem_mmio::unmap(fdt, sizeof(struct fdt_header));
      return;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
  size_t fdt_size = fdt_totalsize(fdt);
#pragma GCC diagnostic pop

  Kmem_mmio::unmap(fdt, sizeof(struct fdt_header));

  // Finally map the entire device tree.
  _fdt = Kmem_mmio::map(fdt_phys, fdt_size, Kmem_mmio::Map_attr::Cached());
  if (!_fdt)
    panic("Cannot map FDT!");
}

IMPLEMENT static inline
void const *
Dt::fdt()
{ return _fdt; }
#endif

