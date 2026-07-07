#pragma once

#include <types.h>
#include <gdt_user_entries.h>
#include <cpu.h>

template<typename BASE>
class Context_cpu_state_arch : public BASE
{
public:
  Gdt_user_entries<4> gdt_user_entries;
  Unsigned16 ds, es, fs, gs;
  Mword gs_base = 0, fs_base = 0;

  explicit Context_cpu_state_arch(Mword *kernel_sp)
  : BASE(kernel_sp)
  {}

  void set_fs_base(Mword base, bool current)
  {
    fs = 0;
    fs_base = base;
    if (current)
      Cpu::set_fs_base(&fs_base);
  }

  void set_gs_base(Mword base, bool current)
  {
    gs = 0;
    gs_base = base;
    if (current)
      Cpu::set_gs_base(&gs_base);
  }

  void store_segments()
  {
    ds = Cpu::get_ds();
    es = Cpu::get_es();
    fs = Cpu::get_fs();
    gs = Cpu::get_gs();
  }

  void load_segments()
  {
    Cpu::set_ds(ds);
    Cpu::set_es(es);
    Cpu::set_fs(fs);
    Cpu::set_gs(gs);

    if (EXPECT_TRUE(!fs))
      Cpu::set_fs_base(&fs_base);

    if (EXPECT_TRUE(!gs))
      Cpu::set_gs_base(&gs_base);
  }

  void vcpu_pv_switch_to_kernel(Vcpu_state *vcpu, bool current)
  {
    fs_base = access_once(&vcpu->host.fs_base);
    gs_base = access_once(&vcpu->host.gs_base);

    vcpu->_regs.ds = ds;
    vcpu->_regs.es = es;
    vcpu->_regs.fs = fs;
    vcpu->_regs.gs = gs;

    unsigned tmp = access_once(&vcpu->host.ds);
    if (EXPECT_FALSE(current && (ds | tmp)))
      Cpu::set_ds(tmp);
    ds = tmp;

    tmp = access_once(&vcpu->host.es);
    if (EXPECT_FALSE(current && (es | tmp)))
      Cpu::set_es(tmp);
    es = tmp;

    tmp = access_once(&vcpu->host.fs);
    if (EXPECT_FALSE(current && (fs | tmp)))
      Cpu::set_fs(tmp);
    fs = tmp;

    if (EXPECT_TRUE(current && !tmp))
      Cpu::set_fs_base(&fs_base);

    tmp = access_once(&vcpu->host.gs);
    if (EXPECT_FALSE(current && (gs | tmp)))
      Cpu::set_gs(tmp);
    gs = tmp;

    if (EXPECT_TRUE(current && !tmp))
      Cpu::set_gs_base(&gs_base);
  }

  void vcpu_pv_switch_to_user(Vcpu_state *vcpu, bool current)
  {
    fs_base = access_once(&vcpu->_regs.fs_base);
    gs_base = access_once(&vcpu->_regs.gs_base);

    unsigned tmp = access_once(&vcpu->_regs.ds);
    if (EXPECT_FALSE(current && (ds | tmp)))
      Cpu::set_ds(tmp);
    ds = tmp;

    tmp = access_once(&vcpu->_regs.es);
    if (EXPECT_FALSE(current && (es | tmp)))
      Cpu::set_es(tmp);
    es = tmp;

    tmp = access_once(&vcpu->_regs.fs);
    if (EXPECT_FALSE(current && (fs | tmp)))
      Cpu::set_fs(tmp);
    fs = tmp;

    if (EXPECT_TRUE(current && !tmp))
      Cpu::set_fs_base(&fs_base);

    tmp = access_once(&vcpu->_regs.gs);
    if (EXPECT_FALSE(current && (gs | tmp)))
      Cpu::set_gs(tmp);
    gs = tmp;

    if (EXPECT_TRUE(current && !tmp))
      Cpu::set_gs_base(&gs_base);
  }

  void switch_segments(Context_cpu_state_arch *to)
  {
    to->gdt_user_entries.load();

    ds = Cpu::get_ds();
    if (EXPECT_FALSE(ds | to->ds))
      Cpu::set_ds(to->ds);

    es = Cpu::get_es();
    if (EXPECT_FALSE(es | to->es))
      Cpu::set_es(to->es);

    fs = Cpu::get_fs();
    if (EXPECT_FALSE(fs | fs_base | to->fs))
      Cpu::set_fs(to->fs);

    if (!to->fs)
      Cpu::set_fs_base(&to->fs_base);

    gs = Cpu::get_gs();
    if (EXPECT_FALSE(gs | gs_base | to->gs))
      Cpu::set_gs(to->gs);

    if (!to->gs)
      Cpu::set_gs_base(&to->gs_base);
  }
};
