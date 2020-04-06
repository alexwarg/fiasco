#include <gic_msi.h>
#include <gic_v3.h>
#include <boot_alloc.h>

Gic_msi::Gic_msi(Gic_v3 *gic, unsigned num_lpis)
: _gic(gic)
{
  _lpis = Lpi_vec(new Boot_object<Lpi>[num_lpis], num_lpis);
  for (unsigned i = 0; i < _lpis.size(); i++)
    _lpis[i].index = i;
}

Irq_base *
Gic_msi::irq(Mword pin) const
{
  if (pin >= _lpis.size())
    return nullptr;

  Lpi const &lpi = _lpis[pin];
  auto g = lock_guard(lpi.lock);
  return _lpis[pin].irq();
}

int
Gic_msi::msg(Mword pin, Unsigned64 src, Irq_mgr::Msi_info *inf)
{
  Msi_src msi_src(src);
  Gic_its *its = _gic->get_its(msi_src.its_num());
  if (!its)
    return -L4_err::ERange;

  int err = 0;
  if (!with_lpi(pin, &Lpi::bind_to_device, its, msi_src.device_id(), inf, err))
    err = -L4_err::ERange;
  return err;
}

void
Gic_msi::migrate_lpis(Cpu_number from, Cpu_number to)
{
  for (Mword pin = 0; pin < _lpis.size(); ++pin)
    {
      Lpi &lpi = _lpis[pin];
      auto g = lock_guard(lpi.lock);
      if (lpi.cpu() == from)
        lpi.set_cpu(to);
    }
}

