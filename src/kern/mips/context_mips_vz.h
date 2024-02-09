#pragma once

#include <vz.h>
#include <vcpu.h>

template<typename CTXT, typename BASE>
class Context_mips_vz : public BASE
{
private:
  using Context = CTXT;

  Context *_this()
  { return static_cast<Context *>(this); }

  Context const *_this() const
  { return static_cast<Context const *>(this); }

  bool is_ext_vcpu() const
  {
    return _this()->state.has(Thread_ext_vcpu_enabled);
  }

protected:
  // must not collide with any bit in Thread_state
  enum { Thread_vcpu_vz_owner = 0x4000000 };

  explicit Context_mips_vz(Mword *kernel_sp)
  : BASE(kernel_sp)
  {}

  void arch_vcpu_ext_shutdown()
  {
    if (!is_ext_vcpu())
      return;

    auto &owner = Vz::owner.current();
    if (owner.ctxt != _this())
      return;

    // If we have a shared guest/root JTLB or VTLB we reset Guest.Wired to 0
    // to allow free use of all TLB entries for root mode.
    Mips::mtgc0_32(0, Mips::Cp0_wired);
    owner.ctxt = 0;
  }

  void vcpu_pv_switch_to_kernel(Vcpu_state *vs, bool current)
  {
    if (!current)
      return;

    auto st = _this()->state();
    if (!(st & Thread_ext_vcpu_enabled))
      return;

    auto *r = _this()->regs();
    if (EXPECT_FALSE(!(r->status & (1UL << 3))))
      return; // not coming from guest

    _this()->vm_state(vs)->save_on_exit(r->cause);
    // LOG_MSG_3VAL(this, "svz", (Mword)s, 0, 0);
  }

  [[gnu::flatten]]
  bool switchin_guest_context(Space *spc)
  {
    bool const guest_context = _this()->state.has(Thread_vcpu_user)
      && (_this()->regs()->status & (1 << 3));

    if (!EXPECT_FALSE(guest_context))
      return false;

    Cpu_number current_cpu = _this()->get_current_cpu();

    if (!EXPECT_TRUE(_this()->home_cpu() == current_cpu))
      return false;

    if (!is_ext_vcpu())
      {
        auto &owner = Vz::owner.cpu(current_cpu);
        Context *owner_ctxt = owner.ctxt;
        if (owner_ctxt)
          owner_ctxt->vz_save_state(owner.guest_id);

        owner.ctxt = _this();
        owner.guest_id = spc->switchin_guest_context();
        vz_load_state(owner.guest_id);
      }
    else
      {
        unsigned guest_id = spc->switchin_guest_context();
        (void)guest_id;
#ifndef NDEBUG
        auto &owner = Vz::owner.cpu(current_cpu);
        assert (owner.ctxt == _this());
        assert ((unsigned)owner.guest_id == guest_id);
#endif
      }
    return true;
  }

public:
  void copy_and_sanitize_trap_state(Trap_state *dst,
                                    Trap_state const *src) const
  {
    *dst = access_once(src);
    dst->status &= Cp0_status::ST_USER_MASK;
    dst->status |= Cp0_status::ST_USER_MUST_SET;
    if (is_ext_vcpu() && (src->status & (1 << 3)))
      dst->status |= 1 << 3;
  }

  static Vz::State *vm_state(Vcpu_state *vs)
  { return offset_cast<Vz::State *>(vs, Config::Ext_vcpu_state_offset); }

  [[gnu::flatten]]
  void vz_save_state(int guest_id)
  {
    vm_state(_this()->vcpu_state().kern())->save_full(guest_id);
    _this()->state.del_dirty(Thread_vcpu_vz_owner);
  }

  [[gnu::flatten]]
  void vz_load_state(int guest_id)
  {
    auto *vm = vm_state(_this()->vcpu_state().kern());
    vm->load_full(guest_id);
    // mark the VM state as dirty as we execute the VM now
    vm->current_cp0_map = 0;
    _this()->state.add_dirty(Thread_vcpu_vz_owner);
  }

};
