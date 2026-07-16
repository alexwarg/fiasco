
#include <device_tree.h>

#include <cassert>
#include <cxx/type_traits>
#include <minmax.h>
#include <string.h>
#include <types.h>

#include <warn.h>

#include <kip.h>
#include <kmem_mmio.h>
#include <panic.h>

Device_tree::Dt Device_tree::dt;

void
Device_tree::init()
{
  Address fdt_phys = 0;
  for (auto const &md: Kip::k()->mem_descs_a())
    {
      if (md.type() == Mem_desc::Info
          && md.ext_type() == Mem_desc::Info_device_tree)
        fdt_phys = md.raw_end();
    }

  if (!fdt_phys)
    return;

  // Map the first page to determine the device tree size.
  void *fdt = Kmem_mmio::map(fdt_phys, sizeof(struct fdt_header), true);
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
  fdt = Kmem_mmio::map(fdt_phys, fdt_size, true);
  if (!fdt)
    panic("Cannot map FDT!");

  dt = Dt(fdt);
}

