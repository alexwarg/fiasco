#include "acpi.h"

#include "boot_alloc.h"
#include "kmem.h"
#include "warn.h"
#include <cctype>


static void
print_acpi_id(char const *id, int len)
{
  char ID[len];
  for (int i = 0; i < len; ++i)
    ID[i] = isalnum(id[i]) ? id[i] : '.';
  printf("%.*s", len, ID);
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
        Acpi_table_head const *t = Kmem::dev_map.map((Acpi_table_head const*)ptrs[i]);
        if (t == (Acpi_table_head const *)~0UL)
          continue;

        if (Acpi::check_signature(t->signature, sig)
            && t->checksum_ok())
          return t;
      }

    return 0;
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

  Acpi_rsdt_p const *rsdt() const
  {
    return (Acpi_rsdt_p const*)(unsigned long)rsdt_phys;
  }

  Acpi_xsdt_p const *xsdt() const
  {
    if (rev == 0)
      return 0;
    return (Acpi_xsdt_p const*)xsdt_phys;
  }


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
    for (unsigned i = 0; i < len && i < 4096; ++i)
      sum += *((Unsigned8 *)this + i);

    return !sum;
  }

  static Acpi_rsdp const *locate();

  void print_info() const
  {
    printf("ACPI: RSDP[%p]\tr%02x OEM:", this, (unsigned)rev);
    print_acpi_id(oem, 6);
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

    return 0;
  }
} __attribute__((packed));


void
Acpi_table_head::print_info() const
{
  printf("ACPI: ");
  print_acpi_id(signature, 4);
  printf("[%p]\tr%02x OEM:", this, (unsigned)rev);
  print_acpi_id(oem_id, 6);
  printf(" OEMTID:");
  print_acpi_id(oem_tid, 8);
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
Acpi::_map_table_head(Unsigned64 phys)
{
  // is the acpi address bigger that our handled physical addresses
  if (Acpi_helper_get_msb<(sizeof(phys) > sizeof(Address))>::msb(phys))
    {
      printf("ACPI: cannot map phys address %llx, out of range (%ubit)\n",
             (unsigned long long)phys, (unsigned)sizeof(Address) * 8);
      return 0;
    }

  void *t = Kmem::dev_map.map((void*)phys);
  if (t == (void *)~0UL)
    {
      printf("ACPI: cannot map phys address %llx, map failed\n",
             (unsigned long long)phys);
      return 0;
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
      Acpi_xsdt_p const *x = Kmem::dev_map.map((const Acpi_xsdt_p *)rsdp->xsdt_phys);
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
            }
          return;
        }
    }

  if (rsdp->rsdt_phys)
    {
      Acpi_rsdt_p const *r = Kmem::dev_map.map((const Acpi_rsdt_p *)(unsigned long)rsdp->rsdt_phys);
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
            }
          return;
        }
    }
}


#if defined (CONFIG_IA32) || defined (CONFIG_AMD64)
// ------------------------------------------------------------------------

#include "panic.h"

Acpi_rsdp const *
Acpi_rsdp::locate()
{
  enum
  {
    ACPI20_PC99_RSDP_START = 0x0e0000,
    ACPI20_PC99_RSDP_END   = 0x100000,

    BDA_EBDA_SEGMENT       = 0x00040E,
  };

  // If we are booted from UEFI, bootstrap reads the RSDP pointer from
  // UEFI and creates a memory descriptor with sub type 5 for it
  for (auto const &md: Kip::k()->mem_descs_a())
    if (   md.type() == Mem_desc::Info
        && md.ext_type() == Mem_desc::Info_acpi_rsdp)
      {
        Acpi_rsdp const *r = Acpi::map_table_head<Acpi_rsdp>(md.start());
        if (   Acpi::check_signature(r->signature, "RSD PTR ")
            && r->checksum_ok())
          return r;
        else
          panic("RSDP memory descriptor from bootstrap invalid");
      }

  if (Acpi_rsdp const *r = locate_in_region(ACPI20_PC99_RSDP_START,
                                            ACPI20_PC99_RSDP_END))
    return r;

  Address ebda = *(Unsigned16 *)BDA_EBDA_SEGMENT << 4;
  if (Acpi_rsdp const *r = locate_in_region(ebda, ebda + 1024))
    return r;

  return 0;
}

#endif // CONFIG_IA32 || CONFIG_AMD64
