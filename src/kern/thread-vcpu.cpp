INTERFACE:
EXTENSION class Thread
{
protected:
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
  Mword arch_check_vcpu_state(bool ext);

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
  void arch_init_vcpu_state(Vcpu_state *vcpu_state, bool ext);
};

IMPLEMENTATION:

#include "logdefs.h"
#include "task.h"
#include "vcpu.h"

IMPLEMENT_DEFAULT inline
Mword Thread::arch_check_vcpu_state(bool)
{ return 0; }

IMPLEMENT_DEFAULT inline
void Thread::arch_init_vcpu_state(Vcpu_state *vcpu, bool /*ext*/)
{
}


