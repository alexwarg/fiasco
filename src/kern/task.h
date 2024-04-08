
#pragma once

#include "context.h"
#include "kobject.h"
#include "l4_types.h"
#include "space.h"
#include "spin_lock.h"
#include "unique_ptr.h"
#include "logdefs.h"
#include "globalconfig.h"

#if defined (CONFIG_JDB)
#include "tb_entry.h"
#endif // CONFIG_JDB

/**
 * \brief A task is a protection domain.
 *
 * A task is derived from Space, which aggregates a set of address spaces.
 * Additionally to a space, a task provides initialization and destruction
 * functionality for a protection domain.
 */
class Task :
  public cxx::Dyn_castable<Task, Kobject>,
  public Space
{
  friend class Jdb_space;

public:
  enum Operation
  {
    Map           = 0,
    Unmap         = 1,
    Cap_info      = 2,
    Add_ku_mem    = 3,
    Ldt_set_x86   = 0x11,
    Vgicc_map_arm = 0x12,
  };

  void operator delete (void *ptr) noexcept;

  /**
   * \brief Create a normal Task.
   * \pre \a parent must be valid and exist.
   */
  explicit Task(Ram_quota *q, Caps c)
  : Space(q, c)
  {
    // increment reference counter from zero
    inc_ref(true);
  }

  explicit Task(Ram_quota *q)
  : Space(q, Caps::mem() | Caps::io() | Caps::obj() | Caps::threads())
  {
    // increment reference counter from zero
    inc_ref(true);
  }

  ~Task() noexcept
  {
    // Task, and thus also its page table, is no longer used anywhere at this
    // point, so no not necessary to flush the freed kernel user memory mappings
    // from the TLB.
    free_ku_mem(false, false);
  }


  template<typename TASK_TYPE, bool MUST_SYNC_KERNEL = true,
           int UTCB_AREA_MR = 0>
  static TASK_TYPE * FIASCO_FLATTEN
  create(Ram_quota *q, L4_msg_tag t, Utcb const *u, int *err);

  template<typename TASK_TYPE, bool MUST_SYNC_KERNEL = false,
           int UTCB_AREA_MR = 0>
  static Kobject_iface * FIASCO_FLATTEN
  generic_factory(Ram_quota *q, Space *, L4_msg_tag t, Utcb const *u, int *err);

  void destroy(Kobject ***reap_list) override;


  bool put() override
  {
    return dec_ref() == 0;
  }

  int alloc_ku_mem(L4_fpage ku_area, bool need_remote_tlb_flush);

  virtual int resume_vcpu(Context *ctxt, Vcpu_state *vcpu, bool user_mode);

  void invoke(L4_obj_ref, L4_fpage::Rights rights, Syscall_frame *f, Utcb *utcb) override;

protected:
  Task(Ram_quota *q, Mem_space::Dir_type* pdir, Caps c)
  : Space(q, pdir, c)
  {
    // increment reference counter from zero
    inc_ref(true);
  }

private:
  /// map the global utcb pointer page into this task
  void map_utcb_ptr_page() {}
  bool invoke_arch(L4_msg_tag &tag, Utcb *utcb);

  int alloc_ku_mem_chunk(User_ptr<void> u_addr, unsigned size, void **k_addr,
                         bool need_remote_tlb_flush);
  void free_ku_mem(Ku_mem *m, bool need_tlb_flush, bool need_remote_tlb_flush);
  void free_ku_mem_chunk(void *k_addr, User_ptr<void> u_addr, unsigned size,
                         unsigned mapped_size, bool need_tlb_flush,
                         bool need_remote_tlb_flush);
  void free_ku_mem(bool need_tlb_flush, bool need_remote_tlb_flush);

  L4_msg_tag sys_map(L4_fpage::Rights rights, Syscall_frame *f, Utcb *utcb);
  L4_msg_tag sys_unmap(Syscall_frame *f, Utcb *utcb);
  L4_msg_tag sys_cap_valid(Syscall_frame *, Utcb *utcb);
  L4_msg_tag sys_caps_equal(Syscall_frame *, Utcb *utcb);
  L4_msg_tag sys_add_ku_mem(Syscall_frame *f, Utcb *utcb);
  L4_msg_tag sys_cap_info(Syscall_frame *f, Utcb *utcb);

#if defined (CONFIG_JDB)
  struct Log_map_unmap : public Tb_entry
  {
    Mword id;
    Mword mask;
    Mword fpage;
    bool  map;
    void print(String_buffer *buf) const;
  };
#endif // CONFIG_JDB
};

#if 0
//---------------------------------------------------------------------------
IMPLEMENTATION:

#include "config.h"
#include "entry_frame.h"
#include "globals.h"
#include "kdb_ke.h"
#include "kmem.h"
#include "kmem_slab.h"
#include "kobject_rpc.h"
#include "l4_types.h"
#include "map_util.h"
#include "mem_layout.h"
#include "ram_quota.h"
#include "thread_state.h"
#include "paging.h"



//---------------------------------------------------------------------------
IMPLEMENTATION:



// ---------------------------------------------------------------------------
INTERFACE [debug]:


EXTENSION class Task
{
private:

};

// ---------------------------------------------------------------------------
IMPLEMENTATION [debug]:
#endif
