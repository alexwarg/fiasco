#include <rv_platforms.h>
#include <cstdio>
#include <cpu.h>

struct Rv_pf_match
{
  Unsigned32 mid;
  Unsigned32 mid_mask;
  Rv_pf const &pf;
};

static constexpr Rv_pf rv_eb =
{
  .sys_r = 0x10000000, .sys_c = 0x10001000,
  .sp804 = 0x10011000,
  .n_gics = 1,
  .gics =
  {
    {
      .version = 2, .primary = true, .offset = 0,
      .dist_phys = 0x10041000, .dist_size = 0x1000,
      .cpu_phys  = 0x10040000, .cpu_size  = 0x100,
    }
  }
};

static constexpr Rv_pf rv_eb_mp =
{
  .scu    = 0x1f000000,
  .sys_r  = 0x10000000, .sys_c = 0x10001000,
  .l2cxx0 = 0x1f002000, .sp804 = 0x10011000,
  .syscon_gic = true,
  .n_gics = 2,
  .gics =
  {
    {
      .version = 2, .primary = true, .offset = 0,
      .dist_phys = 0x1f001000, .dist_size = 0x1000,
      .cpu_phys  = 0x1f000100, .cpu_size  = 0x100,
    },
    {
      .version = 2, .primary = false, .offset = 256, .parent_irq = 42,
      .dist_phys = 0x10041000, .dist_size = 0x1000,
      .cpu_phys  = 0x10040000, .cpu_size  = 0x100,
    },
  }
};

static constexpr Rv_pf rv_pb11mp =
{
  .scu    = 0x1f000000,
  .sys_r  = 0x10000000, .sys_c = 0x10001000,
  .l2cxx0 = 0x1f002000, .sp804 = 0x10011000,
  .syscon_gic = true,
  .n_gics = 1,
  .gics = 
  {
    {
      .version = 2, .primary = true, .offset = 0,
      .dist_phys = 0x1f001000, .dist_size = 0x1000,
      .cpu_phys  = 0x1f000100, .cpu_size  = 0x100,
    },
  }
};

static constexpr Rv_pf rv_pbx_a9 =
{
  .scu    = 0x1f000000,
  .sys_r  = 0x10000000, .sys_c = 0x10001000,
  .l2cxx0 = 0x1f002000, .sp804 = 0x10011000,
  .n_gics = 1,
  .gics =
  {
    {
      .version = 2, .primary = true, .offset = 0,
      .dist_phys = 0x1f001000, .dist_size = 0x1000,
      .cpu_phys  = 0x1f000100, .cpu_size  = 0x100,
    },
  }
};

static constexpr Rv_pf rv_pbx_a8 =
{
  .sys_r = 0x10000000, .sys_c = 0x10001000,
  .sp804 = 0x10011000,
  .n_gics = 1,
  .gics =
  {
    {
      .version = 2, .primary = true, .offset = 0,
      .dist_phys = 0x1e001000, .dist_size = 0x1000,
      .cpu_phys  = 0x1e000000, .cpu_size  = 0x100,
    },
  },
};

static constexpr Rv_pf rv_vexpress_a9 =
{
  .scu   = 0x1e000000,
  .sys_r = 0x10000000, .sys_c = 0x10001000,
  .sp804 = 0x10011000,
  .n_gics = 1,
  .gics = 
  {
    {
      .version = 2, .primary = true, .offset = 0,
      .dist_phys = 0x1e001000, .dist_size = 0x1000,
      .cpu_phys  = 0x1e000100, .cpu_size  = 0x100,
    },
  },
};

static constexpr Rv_pf rv_vexpress =
{
  .sys_r = 0x1c010000, .sys_c = 0x1c020000,
  .n_gics = 1,
  .gics =
  {
    {
      .version = 2, .primary = true, .offset = 0,
      .dist_phys  = 0x2c001000, .dist_size  = 0x1000,
      .cpu_phys   = 0x2c002000, .cpu_size   = 0x2000,
      .cpu_h_phys = 0x2c004000, .cpu_h_size = 0x2000,
      .cpu_v_phys = 0x2c006000, .cpu_v_size = 0x2000,
    },
  },
};


static constexpr Rv_pf_match m_eb[] =
{
  0x4100b020, 0xff00fff0, rv_eb_mp,
  0x4100c090, 0xff00fff0, rv_eb_mp,
  0x0, 0x0, rv_eb,
};

static constexpr Rv_pf_match m_pb[] =
{
  0x4100b020, 0xff00fff0, rv_pb11mp,
  0x4100c090, 0xff00fff0, rv_pbx_a9,
  0x4100c080, 0xff00fff0, rv_pbx_a8,
};

static constexpr Rv_pf_match m_vexpress[] =
{
  0x4100c090, 0xff00fff0, rv_vexpress_a9,
  0, 0, rv_vexpress
};

using V = cxx::static_vector<Rv_pf_match const>;

#ifdef CONFIG_PF_REALVIEW_EB
static V rv_platform_match = m_eb;
#endif
#ifdef CONFIG_PF_REALVIEW_PB11MP
static V rv_platform_match = m_pb;
#endif
#ifdef CONFIG_PF_REALVIEW_PBX
static V rv_platform_match = m_pb;
#endif
#ifdef CONFIG_PF_REALVIEW_VEXPRESS
static V rv_platform_match = m_vexpress;
#endif

static Rv_pf const *current_pf;

Rv_pf const *rv_current_platform()
{
  if (current_pf)
    return current_pf;

  Unsigned32 mid = Cpu::midr();
  for (auto const &a: rv_platform_match)
    {
      if ((mid & a.mid_mask) == a.mid)
        {
          current_pf = &a.pf;
          return current_pf;
        }
    }

  printf("ERROR: nor realview platform match found\n");
  return nullptr;
}

