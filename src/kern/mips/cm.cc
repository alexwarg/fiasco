
#include <cm.h>

#include <boot_alloc.h>
#include <panic.h>
#include <kmem_mmio.h>
#include <mem_layout.h>
#include <mem.h>
#include <cpu.h>

#include <mmio_register_block.h>
#include <cstdio>

Cm *Cm::cm;

template<unsigned REG_WIDTH>
class Cm_x : public Cm
{
public:
  Cm_x(unsigned rev, Phys_mem_addr gcr_phys, Address gcr_base)
  : Cm(rev, gcr_phys), _gcr_base(gcr_base)
  {
    unsigned config = _gcr_base[R_gcr_config];
    printf("MIPS: CM: cores=%u iocus=%u regions=%u\n",
           (config & 0xff) + 1, (config >> 8) & 0xf, (config >> 16) & 0xf);

    if (0)
      {
        printf("MIPS: CM: gcr_base: %x\n", (unsigned)_gcr_base[R_gcr_base]);
        printf("MIPS: CM: control:  %x\n", (unsigned)_gcr_base[R_gcr_control]);
        printf("MIPS: CM: control2: %x\n", (unsigned)_gcr_base[R_gcr_control2]);
        printf("MIPS: CM: access:   %x\n", (unsigned)_gcr_base[R_gcr_access]);
      }
  }

  Address mmio_base() const override
  { return _gcr_base.get_mmio_base(); }

  void set_gic_base_and_enable(Address a) override
  { _gcr_base[R_gcr_gic_base] = a | 1; }


  unsigned num_cores() const
  { return (_gcr_base[R_gcr_config] & 0xff) + 1; }


  void set_co_reset_base(Address base)
  {
    _gcr_base[R_gcr_co + O_gcr_reset_base] = base;
  }

  void set_cl_coherence(Mword ch)
  {
    _gcr_base[R_gcr_cl + O_gcr_coherence] = ch;
  }

  void set_co_coherence(Mword ch)
  {
    _gcr_base[R_gcr_co + O_gcr_coherence] = ch;
  }

  Unsigned32 get_cl_coherence() const
  {
    return _gcr_base[R_gcr_cl + O_gcr_coherence];
  }

  Unsigned32 get_co_coherence() const
  {
    return _gcr_base[R_gcr_co + O_gcr_coherence];
  }

  Unsigned32 del_access(Mword a)
  { return _gcr_base[R_gcr_access].clear(a); }

  Unsigned32 set_access(Mword a)
  { return _gcr_base[R_gcr_access].set(a); }

  void reset_other_core()
  {
    _gcr_base[R_cpc_co + O_cpc_cmd] = 4;
  }

  void power_up_other_core()
  {
    _gcr_base[R_cpc_co + O_cpc_cmd] = 3;
  }

protected:
  Register_block<REG_WIDTH> _gcr_base;

  Cpc_stat_conf get_other_stat_conf() const
  { return Cpc_stat_conf(_gcr_base[R_cpc_co + O_cpc_stat_conf]); }

  Cpc_stat_conf get_stat_conf() const
  { return Cpc_stat_conf(_gcr_base[R_cpc_cl + O_cpc_stat_conf]); }

private:
  void setup_cpc() override;

};


class Cm2 : public Cm_x<32>
{
public:
  Cm2(unsigned revision, Phys_mem_addr phys, Address base)
  : Cm_x<32>(revision, phys, base)
  {}

  void start_all_vps(Address e) override
  {
    unsigned cores = num_cores();
    for (unsigned i = 1; i < cores; ++i)
      {
        set_other_core(i);
        set_co_reset_base(e);
        set_co_coherence(0);
        set_access(1UL << i);
        Mem::sync();
        (void) get_co_coherence();
        reset_other_core();
        Mem::sync();
      }
  }

  unsigned l2_cache_line() const override
  {
    return 0; // L2 cache not managed by CM2
  }

private:
  void set_other_core(Mword core)
  {
    _gcr_base[R_gcr_cl + O_gcr_other] = core << 16;
    if (_cpc_enabled)
      _gcr_base[R_cpc_cl + O_cpc_other] = core << 16;

    Mem::sync();
  }
};

class Cm3 : public Cm_x<MWORD_BITS>
{
public:
  enum Register_cm3
  {
    O_cpc_ctl_reg    = 0x18,
    O_cpc_vp_stop    = 0x20,
    O_cpc_vp_run     = 0x28,
    O_cpc_vp_running = 0x30,
    O_cpc_ram_sleep  = 0x50,
  };

  Cm3(unsigned revision, Phys_mem_addr phys, Address base)
  : Cm_x<MWORD_BITS>(revision, phys, base)
  {}

  void start_all_vps(Address e) override
  {
    Unsigned32 myself = Mips::mfc0_32(Mips::Cp0_global_number);
    unsigned my_vp = myself & 0xff;
    unsigned cores = num_cores();

    for (unsigned i = 0; i < cores; ++i)
      {
        Mword other = redirect(i, 0);
        set_other_core(other);
        set_access(1UL << i);

        auto stat = get_other_stat_conf();
        bool need_reset = false;
        if (stat.seq_state() != 7)
          {
            set_co_coherence(0);
            need_reset = true;
          }

        set_co_reset_base(e);
        unsigned nvps = (_gcr_base[R_gcr_co + O_gcr_config] & 0x3f) + 1;

        for (unsigned v = 1; v < nvps; ++v)
          {
            Mem::sync();
            set_other_core(redirect(i, v));
            set_co_reset_base(e);
          }

        Mem::sync();
        if (nvps > 1)
          set_other_core(redirect(i, 0));

        (void) get_co_coherence();

        unsigned vps_to_reset = (1UL << nvps) - 1;
        if ((myself & ~0x0ff) == redirect(i, 0))
          {
            // start all other VPs on the current core
            vps_to_reset &= ~(1UL << my_vp);
            if (vps_to_reset)
              {
                stop_other_vp(vps_to_reset);
                Mem::sync();
                run_other_vp(vps_to_reset);
              }

            // done with the current core
            continue;
          }

        // stop all VPs on the other core
        stop_other_vp(vps_to_reset);
        Mem::sync();

        // if the other core is not yet in coherent state do a reset
        if (need_reset)
          {
            reset_other_core();
            Mem::sync();
            // make sure all VPs are stopped, I'm not sure if this is needed
            stop_other_vp(vps_to_reset);
            Mem::sync();
          }

        // make sure no VP is running on the core to boot
        while (vps_to_reset & other_running())
          Mem::sync();

        // start VP 0 of the core, this VP will then enable coherency and
        // start the other VPs after it initialized the caches and enabled
        // coherency
        run_other_vp(1); 
        Mem::sync();
      }
  }

  unsigned l2_cache_line() const override
  {
    return (_gcr_base[R_gcr_l2_config] >> 8) & 0xf;
  }

private:
  Mword redirect(unsigned core, unsigned vp)
  { return (core << 8) | vp; }

  void set_other_core(Mword core)
  {
    _gcr_base[R_gcr_cl + O_gcr_other] = core;
    Mem::sync();
  }

  void run_other_vp(unsigned vp)
  { _gcr_base[R_cpc_co + O_cpc_vp_run] = vp; }

  void stop_other_vp(unsigned vp)
  { _gcr_base[R_cpc_co + O_cpc_vp_stop] = vp; }

  Unsigned32 other_running() const
  { return _gcr_base[R_cpc_co + O_cpc_vp_running]; }
};

template<unsigned REG_WIDTH>
void
Cm_x<REG_WIDTH>::setup_cpc()
{
  // if CPC is not available return
  if (!(_gcr_base[R_gcr_cpc_status] & 1))
    return;

  Address v = cxx::int_value<Phys_mem_addr>(_gcr_phys) << 4;

  // set the CPC base address behind the GCR base
  v += 0x8000;
  _gcr_base[R_gcr_cpc_base] = v | 1;

  if (0)
    printf("MIPS: CPC base %lx\n", v);

  if ((_gcr_base[R_gcr_cpc_base] & 1) == 0)
    {
      printf("MIPS: warning could not enable CPC\n");
      return;
    }

  _cpc_enabled = true;
  set_cl_coherence(0xff);

  // paranoia check that the current core enters
  // coherent execution mode
  for (unsigned i = 0; i < 100; ++i)
    {
      asm volatile ("ehb" : : : "memory");
      Mem::sync();
      auto s = get_stat_conf();
      if (s.seq_state() == 7)
        return;
    }

  printf("MIPS: warning boot core did not reach U6 state.\n");
}

void
Cm::init()
{
  if (!Cm::present())
    {
      printf("MIPS: No Coherency Manager (CM) available.\n");
      return;
    }

  Mword v = Mips::mfc0(Mips::Cp0_cmgcr_base);

  // FIXME: allow 36bit remapping of the GCR
  if (v & 0xf0000000)
    panic("GCR base out of range (>32bit phys): %08lx\n", v);

  // FIXME: support some kind of io-reap for the kernel
  if (v >= 0x02000000)
    panic("GCR base out of unmapped KSEG(>512 MiB phys): %08lx\n", v);

  auto gcr_phys = Phys_mem_addr(v);

  Register_block<32> _gcrs(Kmem_mmio::map(v << 4, 0x8000));

  printf("MIPS: Coherency Manager (CM) found: phys=%08lx(<<4) virt=%08lx\n",
         v, _gcrs.get_mmio_base());

  unsigned raw_rev = _gcrs[Cm::R_gcr_rev];
  unsigned rev = raw_rev >> 8;
  printf("MIPS: CM: revision %s (%08x)\n",
         (rev == Rev_cm2) ? "2.0" : (rev == Rev_cm2_5) ? "2.5" :
         (rev == Rev_cm3) ? "3.0" : (rev == Rev_cm3_5) ? "3.5" :
         "<unknown>", raw_rev);

  switch (rev)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case Rev_cm2:
    case Rev_cm2_5:
      cm = new Boot_object<Cm2>(rev, gcr_phys, _gcrs.get_mmio_base());
      break;

    case Rev_cm3:
    case Rev_cm3_5:
      cm = new Boot_object<Cm3>(rev, gcr_phys, _gcrs.get_mmio_base());
      break;

    default:
      printf("MIPS: CM: unknown CM revision, disable CM\n");
      return;
    }

  cm->setup_cpc();
}

