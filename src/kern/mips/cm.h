#pragma once

#include <cxx/bitfield>
#include <cpu.h>
#include <types.h>

class Cm
{
public:
  enum Register
  {
    R_gcr_config            = 0x0000,
    R_gcr_base              = 0x0008,
    R_gcr_control           = 0x0010,
    R_gcr_control2          = 0x0018,
    R_gcr_access            = 0x0020,
    R_gcr_rev               = 0x0030,
    R_gcr_error_mask        = 0x0040,
    R_gcr_error_cause       = 0x0048,
    R_gcr_error_addr        = 0x0050,
    R_gcr_error_mult        = 0x0058,
    R_gcr_custom_base       = 0x0060,
    R_gcr_custom_status     = 0x0068,
    R_gcr_l2_only_sync_base = 0x0070,
    R_gcr_gic_base          = 0x0080,
    R_gcr_cpc_base          = 0x0088,
    R_gcr_reg0_base         = 0x0090,
    R_gcr_reg0_mask         = 0x0098,
    R_gcr_reg1_base         = 0x00a0,
    R_gcr_reg1_mask         = 0x00a8,
    R_gcr_reg2_base         = 0x00b0,
    R_gcr_reg2_mask         = 0x00b8,
    R_gcr_reg3_base         = 0x00c0,
    R_gcr_reg3_mask         = 0x00c8,
    R_gcr_gic_status        = 0x00d0,
    R_gcr_cache_rev         = 0x00e0,
    R_gcr_cpc_status        = 0x00f0,
    R_gcr_l2_config         = 0x0130,
    R_gcr_reg0_attr_base    = 0x0190,
    R_gcr_reg0_attr_mask    = 0x0198,
    R_gcr_reg1_attr_base    = 0x01a0,
    R_gcr_reg1_attr_mask    = 0x01a8,
    R_gcr_iocu1_rev         = 0x0200,
    R_gcr_reg2_attr_base    = 0x0210,
    R_gcr_reg2_attr_mask    = 0x0218,
    R_gcr_reg3_attr_base    = 0x0220,
    R_gcr_reg3_attr_mask    = 0x0228,

    R_gcr_cl                = 0x2000,
    R_gcr_co                = 0x4000,
    O_gcr_coherence         = 0x08,
    O_gcr_config            = 0x10,
    O_gcr_other             = 0x18,
    O_gcr_reset_base        = 0x20,
    O_gcr_id                = 0x28,
    O_gcr_reset_ext_base    = 0x30,
    O_gcr_tcid_0_priority   = 0x40,

    O_gcr_cpc_offset        = 0x8000,
    R_cpc_access            = O_gcr_cpc_offset + 0x000,
    R_cpc_seqdel            = O_gcr_cpc_offset + 0x008,
    R_cpc_rail              = O_gcr_cpc_offset + 0x010,
    R_cpc_resetlen          = O_gcr_cpc_offset + 0x018,
    R_cpc_revision          = O_gcr_cpc_offset + 0x020,
    R_cpc_cl                = O_gcr_cpc_offset + 0x2000,
    R_cpc_co                = O_gcr_cpc_offset + 0x4000,

    O_cpc_cmd               = 0x00,
    O_cpc_stat_conf         = 0x08,
    O_cpc_other             = 0x10,
  };

  enum Cm_revisions
  {
    Rev_cm2 = 6,
    Rev_cm2_5 = 7,
    Rev_cm3 = 8,
    Rev_cm3_5 = 9
  };

  static Cm *cm;

  static bool present()
  {
    return Mips::Configs::read().r<3>().cmgcr();
  }

  static void init();

  Cm(unsigned revision, Phys_mem_addr phys)
  : _gcr_phys(phys), _rev(revision)
  {}

  virtual void start_all_vps(Address e) = 0;
  virtual void set_gic_base_and_enable(Address a) = 0;
  virtual Address mmio_base() const = 0;
  virtual unsigned l2_cache_line() const = 0;

  unsigned revision() const
  { return _rev; }

  bool cpc_present() const
  { return _cpc_enabled; }

protected:
  struct Cpc_stat_conf
  {
    Unsigned32 v;
    Cpc_stat_conf() = default;
    explicit Cpc_stat_conf(Unsigned32 v) : v(v) {}

    CXX_BITFIELD_MEMBER( 0,  3, cmd, v);
    CXX_BITFIELD_MEMBER( 4,  4, io_trffc_en, v);
    CXX_BITFIELD_MEMBER( 7,  7, reset_hold, v);     // CM3
    CXX_BITFIELD_MEMBER( 8,  9, pwup_policy, v);
    CXX_BITFIELD_MEMBER(10, 10, lpack, v);          // CM3
    CXX_BITFIELD_MEMBER(11, 11, coh_en, v);         // CM3
    CXX_BITFIELD_MEMBER(12, 12, ci_rail_stable, v); // CM3
    CXX_BITFIELD_MEMBER(13, 13, ci_vdd_ok, v);      // CM3
    CXX_BITFIELD_MEMBER(14, 14, ci_pwrup, v);       // CM3
    CXX_BITFIELD_MEMBER(15, 15, ejtag_probe, v);
    CXX_BITFIELD_MEMBER(16, 16, pwrdn_impl, v);
    CXX_BITFIELD_MEMBER(17, 17, clkgat_impl, v);
    CXX_BITFIELD_MEMBER(19, 22, seq_state, v);
    CXX_BITFIELD_MEMBER(23, 23, pwrup_event, v);
    CXX_BITFIELD_MEMBER(24, 24, l2_hw_init_en, v);
  };

  virtual void setup_cpc() = 0;

  Phys_mem_addr _gcr_phys;
  unsigned _rev;
  bool _cpc_enabled = false;
};

