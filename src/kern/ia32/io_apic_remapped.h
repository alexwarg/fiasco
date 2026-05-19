#pragma once

#include <io_apic.h>

namespace Intel { class Io_mmu; }

class Io_apic_remapped : public Io_apic
{
public:
  IRQ_CHIP_DBG_INFO("rIO-APIC");

  Io_apic_remapped(Unsigned64 phys, unsigned gsi_base,
                   Intel::Io_mmu *iommu, Unsigned16 src_id)
  : Io_apic(phys, gsi_base), _iommu(iommu), _src_id(src_id)
  {}

  bool alloc(Irq_base *irq, Mword pin, bool init = true) override;
  void unbind(Irq_base *irq) override;
  int set_mode(Mword pin, Mode mode) override;
  void set_cpu(Mword pin, Cpu_number cpu) override;

  static bool init_apics();

private:
  Intel::Io_mmu *_iommu;
  Unsigned16 _src_id;
};


