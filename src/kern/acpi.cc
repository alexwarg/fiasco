#include "acpi.h"

#include "boot_alloc.h"
#include "kmem.h"
#include "warn.h"
#include "panic.h"
#include <cctype>


template<unsigned LEN>
static void
print_acpi_id(char const (&id)[LEN])
{
  char id_str[LEN];
  for (unsigned i = 0; i < LEN; ++i)
    id_str[i] = isalnum(id[i]) ? id[i] : '.';
  printf("%.*s", LEN, id_str);
}


namespace Acpi {
  static Acpi_sdt _sdt;
  static bool _init_done;

  Acpi_sdt const *sdt()
  { return &_sdt; }

  bool check_signature(char const *sig, char const *reference)
  {
    for (; *reference; ++sig, ++reference)
      if (*reference != *sig)
        return false;

    return true;
  }
}

template< typename T >
class Acpi_sdt_p : public Acpi_table_head
{
public:
  T ptrs[0];

  unsigned entries() const
  { return (len - sizeof(Acpi_table_head)) / sizeof(ptrs[0]); }

  Acpi_table_head const *find(char const *sig) const
  {
    for (unsigned i = 0; i < ((len-sizeof(Acpi_table_head))/sizeof(ptrs[0])); ++i)
      {
        Acpi_table_head const *t = Kmem::mmio_remap(ptrs[i], sizeof(*t), true);
        if (t == (Acpi_table_head const *)~0UL)
          continue;

        if (Acpi::check_signature(t->signature, sig)
            && t->checksum_ok())
          return t;
      }

    return nullptr;
  }

} __attribute__((packed));

typedef Acpi_sdt_p<Unsigned32> Acpi_rsdt_p;
typedef Acpi_sdt_p<Unsigned64> Acpi_xsdt_p;

class Acpi_rsdp
{
public:
  char       signature[8];
  Unsigned8  chk_sum;
  char       oem[6];
  Unsigned8  rev;
  Unsigned32 rsdt_phys;
  Unsigned32 len;
  Unsigned64 xsdt_phys;
  Unsigned8  ext_chk_sum;
  char       reserved[3];

  bool checksum_ok() const
  {
    // ACPI 1.0 checksum
    Unsigned8 sum = 0;
    for (unsigned i = 0; i < 20; i++)
      sum += *((Unsigned8 *)this + i);

    if (sum)
      return false;

    if (rev == 0)
      return true;

    // Extended Checksum
    for (unsigned i = 0; i < len; ++i)
      sum += *((Unsigned8 *)this + i);

    return !sum;
  }

  static Acpi_rsdp const *locate();

  void print_info() const
  {
    printf("ACPI: RSDP[%p]\tr%02x OEM:", this, (unsigned)rev);
    print_acpi_id(oem);
    printf("\n");
  }

  static Acpi_rsdp const *locate_in_region(Address start, Address end)
  {
    for (Address p = start; p < end; p += 16)
      {
        Acpi_rsdp const* r = (Acpi_rsdp const *)p;
        if (Acpi::check_signature(r->signature, "RSD PTR ")
            && r->checksum_ok())
          return r;
      }

    return nullptr;
  }
} __attribute__((packed));


void
Acpi_table_head::print_info() const
{
  printf("ACPI: ");
  print_acpi_id(signature);
  printf("[%p]\tr%02x OEM:", this, (unsigned)rev);
  print_acpi_id(oem_id);
  printf(" OEMTID:");
  print_acpi_id(oem_tid);
  printf("\n");
}

void
Acpi_sdt::print_summary() const
{
  for (unsigned i = 0; i < _num_tables; ++i)
    if (_tables[i])
      _tables[i]->print_info();
}


void *
Acpi::_map_table(Unsigned64 phys, unsigned size)
{
  // is the acpi address bigger that our handled physical addresses
  if (Acpi_helper_get_msb < (sizeof(phys) > sizeof(Address))>::msb(phys + size - 1))
    {
      printf("ACPI: cannot map phys address %llx, out of range (%ubit)\n",
             (unsigned long long)phys, (unsigned)sizeof(Address) * 8);
      return nullptr;
    }

  void *t = (void *)Kmem::mmio_remap(phys, size, true);
  if (t == (void *)~0UL)
    {
      printf("ACPI: cannot map phys address %llx, map failed\n",
             (unsigned long long)phys);
      return nullptr;
    }

  return t;
}


void
Acpi::init_virt()
{
  enum { Print_info = 0 };

  if (_init_done)
    return;
  _init_done = 1;

  if (Print_info)
    printf("ACPI-Init\n");

  Acpi_rsdp const *rsdp = Acpi_rsdp::locate();
  if (!rsdp)
    {
      WARN("ACPI: Could not find RSDP, skip init\n");
      return;
    }

  rsdp->print_info();

  if (rsdp->rev && rsdp->xsdt_phys)
    {
      Acpi_xsdt_p const *x = (Acpi_xsdt_p const *)Kmem::mmio_remap(rsdp->xsdt_phys, sizeof(*x), true);
      if (x == (Acpi_xsdt_p const *)~0UL)
        WARN("ACPI: Could not map XSDT\n");
      else if (!x->checksum_ok())
        WARN("ACPI: Checksum mismatch in XSDT\n");
      else
        {
          _sdt.init(x);
          if (Print_info)
            {
              x->print_info();
              _sdt.print_summary();

              Acpi_srat const *srat = Acpi::find<Acpi_srat const *>("SRAT");
              if (srat)
                srat->show();
            }
          return;
        }
    }

  if (rsdp->rsdt_phys)
    {
      Acpi_rsdt_p const *r = (Acpi_rsdt_p const *)Kmem::mmio_remap(rsdp->rsdt_phys, sizeof(*r), true);
      if (r == (Acpi_rsdt_p const *)~0UL)
        WARN("ACPI: Could not map RSDT\n");
      else if (!r->checksum_ok())
        WARN("ACPI: Checksum mismatch in RSDT\n");
      else
        {
          _sdt.init(r);
          if (Print_info)
            {
              r->print_info();
              _sdt.print_summary();

              Acpi_srat const *srat = Acpi::find<Acpi_srat const *>("SRAT");
              if (srat)
                srat->show();
            }
          return;
        }
    }
}

static
Acpi_rsdp const *locate_via_kip()
{
  // If we are booted from UEFI, bootstrap reads the RSDP pointer from
  // UEFI and creates a memory descriptor with sub type 5 for it
  for (auto const &md: Kip::k()->mem_descs_a())
    if (   md.type() == Mem_desc::Info
        && md.ext_type() == Mem_desc::Info_acpi_rsdp)
      {
        // Rover across first page in memory descriptor. The actual RSDP
        // address was rounded down to page alignment by bootstrap...
        for (unsigned off = 0; off < Config::PAGE_SIZE; off += sizeof(void*))
          {
            Acpi_rsdp const *r = Acpi::map_table_head<Acpi_rsdp>(md.start() + off);
            if (   Acpi::check_signature(r->signature, "RSD PTR ")
                && r->checksum_ok())
              return r;
          }

        panic("RSDP memory descriptor from bootstrap invalid");
      }

  return nullptr;
}

void
Acpi_srat::show() const
{
  printf("SRAT Table Revision: 0x%x (len: %d)\n", table_revision, len);

  for (unsigned i = 0; i < len - sizeof(Acpi_srat);)
    {
      auto *h = reinterpret_cast<Acpi_subtable_header const *>(data + i);
      switch (h->type)
        {
        case Type::Cpu_affinity:
          {
            auto *c = reinterpret_cast<Cpu_affinity const *>(data + i);
            if (c->flags & Cpu_use_affinity)
              printf("  Cpu: prox_dom_lo,hi[3]=%x,%x,%x,%x apic_id=0x%x local_sapic_eid=0x%x clk_dom=0x%x\n",
                     c->proximity_domain_lo, c->proximity_domain_hi[0],
                     c->proximity_domain_hi[1], c->proximity_domain_hi[2],
                     c->apic_id, c->local_sapic_eid, c->clock_domain);
            break;
          }
        case Type::Memory_affinity:
          {
            auto *m = reinterpret_cast<Mem_affinity const *>(data + i);
            if (m->flags & Mem_enabled)
              printf("  Mem: Proxdomain=%d Baseaddr=%llx Len=%llx %s %s\n",
                     m->proximity_domain, m->base_address, m->length,
                     (m->flags & Mem_hot_pluggable) ? "HotPluggable" : "Noplug",
                     (m->flags & Mem_non_volatile) ? "NonVolatile" : "Forgetting");
            break;
          }
        case Type::X2APIC_cpu_affinity:
          {
            auto *p = reinterpret_cast<Proc_lapic2 const *>(data + i);
            printf("  Cpu: domain=0x%x x2apic_id=0x%x flags=0x%x clk_dom=0x%x\n",
                   p->domain, p->x2apic_id, p->flags, p->clock_domain);
            break;
          }
        default:
          printf("Unhandled SRAT type %d\n", h->type);
          break;
        }

      i += h->len;
    }
}


#if defined (CONFIG_IA32) || defined (CONFIG_AMD64)
// ------------------------------------------------------------------------

Acpi_rsdp const *
Acpi_rsdp::locate()
{
  enum
  {
    ACPI20_PC99_RSDP_START = 0x0e0000,
    ACPI20_PC99_RSDP_END   = 0x100000,

    BDA_EBDA_SEGMENT       = 0x00040E,
  };

  if (Acpi_rsdp const *r = locate_via_kip())
    return r;

  if (Acpi_rsdp const *r = locate_in_region(ACPI20_PC99_RSDP_START,
                                            ACPI20_PC99_RSDP_END))
    return r;

  extern char ebda_segment[];

  Address ebda = *(Unsigned16 const *)ebda_segment << 4;
  if (Acpi_rsdp const *r = locate_in_region(ebda, ebda + 1024))
    return r;

  return nullptr;
}

#endif // CONFIG_IA32 || CONFIG_AMD64
