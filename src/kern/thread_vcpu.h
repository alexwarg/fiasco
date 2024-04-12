#pragma once

#include <context.h>
#include <vcpu_log.h>
#include <logdefs.h>
#include <thread_vcpu_arch.h>

class Thread_vcpu_generic
{
public:
  static bool
  vcpu_pagefault(Context *c, Address pfa, Mword err, Mword ip)
  {
    (void)ip;
    Vcpu_state *vcpu = c->vcpu_state().access();
    if (!c->vcpu_pagefaults_enabled(vcpu))
      return false;

    c->spill_user_state();
    c->vcpu_enter_kernel_mode(vcpu);
    LOG_TRACE("VCPU events", "vcpu", c, Vcpu_log,
        l->type = 3;
        l->state = vcpu->saved_state();
        l->ip = ip;
        l->sp = pfa;
        l->err = err;
        l->space = c->vcpu_user_space()
                   ? static_cast<Task*>(c->vcpu_user_space())->dbg_id()
                   : ~0;
        );
    vcpu->_regs.s.set_pagefault(pfa, err);
    c->vcpu_save_state_and_upcall();
    return true;
  }

  static bool ext_vcpu_available() { return false; }

  /**
   * Check that everything is ready to initialize the vCPU state.
   *
   * Run any architecture-specific preparation steps and checks to make sure
   * that the thread vCPU state can be initialized and enabled.
   *
   * If this method indicates success, then the caller is expected to
   * initialize and enable the vCPU state.
   *
   * \param[in] ext  Indicate whether the extended vCPU state is about to be
   *                 initialized and enabled.
   *
   * \return Status of the preparation steps and checks. The value 0 indicates
   *         success, any other value indicates a specific error.
   */
  static int pre_check(Context *c, bool ext)
  {
    (void)c; (void)ext;
    return 0;
  }

  /**
   * Initialize the vCPU state.
   *
   * Architecture-specific vCPU state initialization. This method is called
   * after the successful call to \ref arch_check_vcpu_state and after the
   * \ref _vcpu_state member has been set to the vCPU state, but before the
   * vCPU state is finally enabled.
   *
   * This method is not expected to fail.
   *
   * \param[in,out] vcpu_state  vCPU state to initialize.
   * \param[in]     ext         Indicate whether the extended vCPU state needs
   *                            to be initialized.
   */
  static void init_state(Context *, Vcpu_state *, bool) {}
};

using Thread_vcpu = Thread_vcpu_arch_t<Thread_vcpu_generic>;

