#pragma once

template<typename CTXT, typename BASE>
class Context_mips_vz : public BASE
{
protected:
  explicit Context_mips_vz(Mword *kernel_sp)
  : BASE(kernel_sp)
  {}

  bool switchin_guest_context(Space *) const
  { return false; }

public:
  void copy_and_sanitize_trap_state(Trap_state *dst,
                                    Trap_state const *src) const
  { dst->copy_and_sanitize(src); }

  void vz_save_state(int) const
  {}

  void vz_load_state(int) const
  {}

  void vcpu_pv_switch_to_kernel(Vcpu_state *, bool)
  {}
};
