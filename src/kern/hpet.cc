
#include "hpet.h"
#include "initcalls.h"
#include "mem_layout.h"
#include "kmem.h"
#include "kip.h"

#include <cassert>

Acpi_hpet const *Hpet::_acpi_hpet;
Hpet_device *Hpet::_hpet;
Hpet_timer *Hpet::_hpet_timer;

void
Hpet_device::dump() const
{
  printf("HPET: cap+id : %016llx\n", cap_and_id);
  printf("      clk-per: %d femtosecs\n", counter_clk_period());
  printf("      gen-config: %llx\n", config);
}

void
Hpet::dump_acpi_infos()
{
  if (!_acpi_hpet)
    return;

  printf("ACPI-HPET = %p\n", _acpi_hpet);

  printf("  event_timer_block_id:    %x\n", _acpi_hpet->event_timer_block_id);
  printf("  base_address:            %llx (as: %d, off: %d, w: %d, id: %d)\n",
         _acpi_hpet->base_address.addr,
         _acpi_hpet->base_address.access_size,
         _acpi_hpet->base_address.offset,
         _acpi_hpet->base_address.width,
         _acpi_hpet->base_address.id);
  printf("  hpet_number:             %d\n", _acpi_hpet->hpet_number);
  printf("  min_clock_ticks_for_irq: %d\n", _acpi_hpet->min_clock_ticks_for_irq);
  printf("  page_prot_and_oem_attr:  %x\n", _acpi_hpet->page_prot_and_oem_attr);
}

FIASCO_INIT_CPU_AND_PM
bool
Hpet::init()
{
  _acpi_hpet = Acpi::find<Acpi_hpet const *>("HPET");

  if (!_acpi_hpet)
    {
      printf("Could not find HPET in RSDT nor XSDT, skipping init\n");
      return false;
    }

  dump_acpi_infos();

  Address offs;
  Address a = _acpi_hpet->base_address.addr;
  Address va = Mem_layout::alloc_io_vmem(Config::PAGE_SIZE);
  assert (va);

  Kmem::map_phys_page(a, va, false, true, &offs);

  Kip::k()->add_mem_region(Mem_desc(a, a + 1023, Mem_desc::Reserved));

  _hpet = offset_cast<Hpet_device *>(va, offs);

  _hpet->dump();

  _hpet->disable();
  _hpet->reset_counter();

  int i = 2;
  Hpet_timer *t = 0;
  for (; i < _hpet->num_timers(); ++i)
    {
      t = _hpet->timer(i);

      if (t->can_periodic() && t->int_route_cap())
        {
          t->force_32bit();
          t->set_periodic();
          t->set_edge_irq();
          t->val_set();
          t->set_int(t->get_first_int());

          t->comp_val = div32(1000000000000ULL, _hpet->counter_clk_period());

          break;
        }
    }

  if (!t)
    {
      printf("ERROR: Did not find a HPET timer that can do periodic mode.\n");
      return false;
    }

  _hpet_timer = t;

  _hpet->dump();

  return true;
}

