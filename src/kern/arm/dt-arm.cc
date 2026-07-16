
#include <dt-arm.h>
#include <device_tree.h>

#ifdef CONFIG_DT

static unsigned
decode_irq_cells(unsigned type, unsigned number)
{
  if (type == 0)
    return number + 32;
  if (type == 1)
    return number + 16;
  return ~0u;
}

unsigned
Dt_arm::get_gic_irq(Device_tree::Node n, unsigned idx)
{
  // Try 'interrupts-extended' first: 4 cells per entry (phandle, type, number, flags)
  Device_tree::Array ext = n.get_array("interrupts-extended");
  if (ext.is_present())
    {
      unsigned base = idx * 4;
      if (base + 4 <= ext.len())
        return decode_irq_cells(ext.get<unsigned>(base + 1),
                                ext.get<unsigned>(base + 2));
      return ~0u;
    }

  // Fall back to 'interrupts': 3 cells per entry (type, number, flags)
  Device_tree::Array irqs = n.get_array("interrupts");
  if (irqs.is_present())
    {
      unsigned base = idx * 3;
      if (base + 2 <= irqs.len())
        return decode_irq_cells(irqs.get<unsigned>(base),
                                irqs.get<unsigned>(base + 1));
    }

  return ~0u;
}

unsigned
Dt_arm::get_gic_irq(Device_tree::Node n, const char *name)
{
  void const *fdt = n.get_fdt();
  int off = n.get_off();
  if (!fdt || off < 0)
    return ~0u;

  int count = fdt_stringlist_count(fdt, off, "interrupt-names");
  if (count < 0)
    return ~0u;

  for (int i = 0; i < count; i++)
    {
      int len;
      const char *s = fdt_stringlist_get(fdt, off, "interrupt-names", i, &len);
      if (s && len > 0 && strcmp(s, name) == 0)
        return get_gic_irq(n, static_cast<unsigned>(i));
    }

  return ~0u;
}

#else

unsigned Dt_arm::get_gic_irq(Device_tree::Node, unsigned) { return ~0u; }
unsigned Dt_arm::get_gic_irq(Device_tree::Node, const char *) { return ~0u; }

#endif
