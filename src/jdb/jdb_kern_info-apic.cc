
#include <jdb_kern_info.h>
#include <cstdio>
#include "simpleio.h"

#include "apic.h"
#include "static_init.h"

class Jdb_kern_info_apic : public Jdb_kern_info_module
{
public:
  Jdb_kern_info_apic()
    : Jdb_kern_info_module('a', "Local APIC state")
  {
    Jdb_kern_info::register_subcmd(this);
  }

  void show() override;
};

static Jdb_kern_info_apic k_a INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);

static void apic_id_show(int indent = 0)
{
  printf("%*sAPIC id: %02x version: %02x\n",
         indent, "", get_id() >> 24, get_version());
}

static void apic_timer_show(int indet = 0)
{
  printf("%*sTimer mode: %s  counter: %08x/%08x\n",
         indent, "",
	 Apic::reg_read(Apic::APIC_lvtt) & Apic::APIC_lvt_timer_periodic
	   ? "periodic" : "one-shot",
	 Apic::timer_reg_read_initial(), Apic::timer_reg_read());
}

static const char* reg_lvt_bit_str(unsigned reg, Unsigned32 val, int bit)
{
  static const char * const delivery_mode[] =
    { "fixed", "???", "SMI", "???", "NMI", "INIT", "???", "ExtINT" };
  unsigned bits = 0;

  switch (reg)
    {
    case Apic::APIC_lvtt:
      bits = Apic::Mask | Apic::Delivery_state;
      break;
    case Apic::APIC_lvt0:
    case Apic::APIC_lvt1:
      bits = Apic::Mask | Apic::Trigger_mode | Apic::Remote_irr | Apic::Pin_polarity
        | Apic::Delivery_state | Apic::Delivery_mode;
      break;
    case Apic::APIC_lvterr:
      bits = Apic::Mask | Apic::Delivery_state;
      break;
    case Apic::APIC_lvtpc:
      bits = Apic::Mask | Apic::Delivery_state | Apic::Trigger_mode;
      break;
    case Apic::APIC_lvtthmr:
      bits = Apic::Mask | Apic::Delivery_state | Apic::Trigger_mode;
      break;
    }

  if ((bits & bit) == 0)
    return "";

  switch (bit)
    {
    case Apic::Mask:
      return val & Apic::APIC_lvt_masked ? "masked" : "unmasked";
    case Apic::Trigger_mode:
      return val & Apic::APIC_lvt_level_trigger ? "level" : "edge";
    case Apic::Remote_irr:
      return val & Apic::APIC_lvt_remote_irr ? "IRR" : "";
    case Apic::Pin_polarity:
      return val & Apic::APIC_input_polarity ? "active low" : "active high";
    case Apic::Delivery_state:
      return val & Apic::APIC_snd_pending ? "pending" : "idle";
    case Apic::Delivery_mode:
      return delivery_mode[Apic::reg_delivery_mode(val)];
    }

  return "";
}


static void apic_reg_show(unsigned reg)
{
  Unsigned32 tmp_val = Apic::reg_read(reg);

  printf("%-9s%-6s%-4s%-8s%-7s%02x",
         reg_lvt_bit_str(reg, tmp_val, Apic::Mask),
         reg_lvt_bit_str(reg, tmp_val, Apic::Trigger_mode),
         reg_lvt_bit_str(reg, tmp_val, Apic::Remote_irr),
         reg_lvt_bit_str(reg, tmp_val, Apic::Delivery_state),
         reg_lvt_bit_str(reg, tmp_val, Apic::Delivery_mode),
         (unsigned)Apic::reg_lvt_vector(tmp_val));
}


static void apic_regs_show(int indent = 0)
{
  printf("%*sVectors:   LINT0: ", indent, ""); reg_show(APIC_lvt0);
  printf("\n%*sLINT1: ", indent + 11, ""); reg_show(APIC_lvt1);
  printf("\n%*sTimer: ", indent + 11, ""); reg_show(APIC_lvtt);
  printf("\n%*sError: ", indent + 11, ""); reg_show(APIC_lvterr);
  if (Apic::have_pcint())
    {
      printf("\n%*sPerfCnt: ", indent + 9, "");
      apic_reg_show(Apic::APIC_lvtpc);
    }
  if (Apic::have_tsint())
    {
      printf("\n%*sThermal: ", indent + 9, "");
      apic_reg_show(Apic::APIC_lvtthmr);
    }
  putchar('\n');
}

static void apic_bitfield_show(unsigned reg, const char *name, char flag, int indent)
{
  unsigned i, j;
  Unsigned32 tmp_val;

  printf("%*s%-11s    0123456789abcdef0123456789abcdef"
                     "0123456789abcdef0123456789abcdef\n", indent, "", name);
  for (i=0; i<8; i++)
    {
      if (!(i & 1))
	printf("%*s%02x ", indent + 12, "", i*0x20);
      tmp_val = Apic::reg_read(reg + i*0x10);
      for (j=0; j<32; j++)
	putchar(tmp_val & (1<<j) ? flag : '.');
      if (i & 1)
	putchar('\n');
    }
}

static void apic_irr_show(int indent = 0)
{
  apic_bitfield_show(Apic::APIC_irr, "Ints Reqst:", 'R', indent);
}

static void apic_isr_show(int indent = 0)
{
  apic_bitfield_show(Apic::APIC_isr, "Ints InSrv:", 'S', indent);
}


void
Jdb_kern_info_apic::show()
{
  if (!Config::apic)
    {
      puts("Local APIC disabled/not available");
      return;
    }

  for (Cpu_number u = Cpu_number::first(); u < Config::max_num_cpus(); ++u)
    if (Cpu::online(u))
      {
        printf("CPU%u: ", cxx::int_value<Cpu_number>(u));
        auto show_info = [](Cpu_number)
          {
            Apic::id_show(0);
            Apic::timer_show(4);
            Apic::regs_show(4);
            Apic::irr_show(4);
            Apic::isr_show(4);
            putchar('\n');
          };

        if (u == Cpu_number::boot_cpu())
          show_info(u);
        else
          Jdb::remote_work(u, show_info, true);
      }
}

