#pragma once

#include <types.h>
#include "boot_alloc.h"

#include <cstdio>

class Acpi_sdt;

namespace Acpi {
  void *_map_table_head(Unsigned64 phys);

  template<typename TAB>
  inline TAB *map_table_head(Unsigned64 phys)
  { return reinterpret_cast<TAB *>(_map_table_head(phys)); }

  Acpi_sdt const *sdt();
  bool check_signature(char const *sig, char const *reference);
  void init_virt();

};

class Acpi_gas
{
public:
  enum Type { System_mem = 0, System_io = 1, Pci_cfg_mem = 2 };
  Unsigned8  id;
  Unsigned8  width;
  Unsigned8  offset;
  Unsigned8  access_size;
  Unsigned64 addr;
} __attribute__((packed));



class Acpi_table_head
{
public:
  char       signature[4];
  Unsigned32 len;
  Unsigned8  rev;
  Unsigned8  chk_sum;
  char       oem_id[6];
  char       oem_tid[8];
  Unsigned32 oem_rev;
  Unsigned32 creator_id;
  Unsigned32 creator_rev;

  bool checksum_ok() const
  {
    Unsigned8 sum = 0;
    for (unsigned i = 0; i < len; ++i)
      sum += *((Unsigned8 *)this + i);

    return !sum;
  }

  void print_info() const;

} __attribute__((packed));


class Acpi_sdt
{
public:
  void print_summary() const;

  template< typename SDT >
  void init(SDT *sdt)
  {
    unsigned entries = sdt->entries();
    _tables = (Acpi_table_head const **)Boot_alloced::alloc(sizeof(*_tables) * entries);
    if (_tables)
      {
        _num_tables = entries;
        for (unsigned i = 0; i < entries; ++i)
          if (sdt->ptrs[i])
            _tables[i] = this->map_entry(i, sdt->ptrs[i]);
          else
            _tables[i] = 0;
      }
  }

  template< typename T >
  Acpi_table_head const *map_entry(unsigned idx, T phys)
  {
    if (idx >= _num_tables)
      {
        printf("ACPI: table index out of range (%u >= %u)\n", idx, _num_tables);
        return 0;
      }

    return Acpi::map_table_head<Acpi_table_head>((Unsigned64)phys);
  }

  Acpi_table_head const *find(char const *sig) const
  {
    for (unsigned i = 0; i < _num_tables; ++i)
      {
        Acpi_table_head const *t = _tables[i];
        if (!t)
          continue;

        if (Acpi::check_signature(t->signature, sig)
            && t->checksum_ok())
          return t;
      }

    return 0;
  }

private:
  unsigned _num_tables;
  Acpi_table_head const **_tables;
};

namespace Acpi {
  template< typename T >
  T find(const char *s)
  {
    init_virt();
    return static_cast<T>(sdt()->find(s));
  }
}


class Acpi_madt : public Acpi_table_head
{
public:
  enum Type
  { LAPIC, IOAPIC, Irq_src_ovr, NMI, LAPIC_NMI, LAPIC_adr_ovr, IOSAPIC,
    LSAPIC, Irq_src,
    GICC = 0xb, GICD, GICM, GICR, ITS,
  };

  struct Apic_head
  {
    Unsigned8 type;
    Unsigned8 len;
  } __attribute__((packed));

  struct Lapic : public Apic_head
  {
    enum { ID = LAPIC };
    Unsigned8 apic_processor_id;
    Unsigned8 apic_id;
    Unsigned32 flags;
  } __attribute__((packed));

  struct Io_apic : public Apic_head
  {
    enum { ID = IOAPIC };
    Unsigned8 id;
    Unsigned8 res;
    Unsigned32 adr;
    Unsigned32 irq_base;
  } __attribute__((packed));

  struct Irq_source : public Apic_head
  {
    enum { ID = Irq_src_ovr };
    Unsigned8  bus;
    Unsigned8  src;
    Unsigned32 irq;
    Unsigned16 flags;
  } __attribute__((packed));

  struct Gic_cpu_if : public Apic_head
  {
    enum { ID = GICC };
    enum { Enabled = 0x1, Perf_irq_edge = 0x2, Vgic_irq_edge = 0x4 };
    Unsigned8 reserved[2];
    Unsigned32 cpu_if_num;
    Unsigned32 uid;
    Unsigned32 flags;
    Unsigned32 parking_protocol_version;
    Unsigned32 perf_gsiv;
    Unsigned64 parked_addr;
    Unsigned64 gicc_base;
    Unsigned64 gicv_base;
    Unsigned64 gich_base;
    Unsigned32 vgic_maintenance_irq;
    Unsigned64 gicr_base;
    Unsigned64 mpidr;
    Unsigned8  power_efficiency_class;
    Unsigned8  reserved2;
    Unsigned16 spe_overflow_irq;
  } __attribute__((packed));

  struct Gic_distributor_if : public Apic_head
  {
    enum { ID = GICD };
    enum { V1 = 1, V2, V3, V4 };
    Unsigned8   reserved[2];
    Unsigned32  id;
    Unsigned64  base;
    Unsigned32  reserved2;
    Unsigned8   version;
  } __attribute__((packed));

  struct Gic_redistributor_if : public Apic_head
  {
    enum { ID = GICR };
    Unsigned8   reserved[2];
    Unsigned64  base;
    Unsigned32  length;
  } __attribute__((packed));

  struct Gic_its_if : public Apic_head
  {
    enum { ID = ITS };
    Unsigned8   reserved[2];
    Unsigned32  id;
    Unsigned64  base;
  } __attribute__((packed));

  Apic_head const *find(Unsigned8 type, int idx) const
  {
    for (unsigned i = 0; i < len-sizeof(Acpi_madt);)
      {
        Apic_head const *a = (Apic_head const *)(data + i);
        //printf("a=%p, a->type=%u, a->len=%u\n", a, a->type, a->len);
        if (a->type == type)
          {
            if (!idx)
              return a;
            --idx;
          }
        i += a->len;
      }

    return 0;
  }

  template<typename T>
  T const *find(int idx) const
  {
    return static_cast<T const *>(find(T::ID, idx));
  }

public:
  Unsigned32 local_apic;
  Unsigned32 apic_flags;

private:
  char data[0];
} __attribute__((packed));

template< bool >
struct Acpi_helper_get_msb
{ template<typename P> static Address msb(P) { return 0; } };

template<>
struct Acpi_helper_get_msb<true>
{ template<typename P> static Address msb(P p) { return p >> (sizeof(Address) * 8); } };

