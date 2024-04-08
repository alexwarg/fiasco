#pragma once

#include "task.h"
#include "kobject_rpc.h"
#include "kmem_slab.h"

template<typename TASK_TYPE, bool MUST_SYNC_KERNEL, int UTCB_AREA_MR>
TASK_TYPE * FIASCO_FLATTEN
Task::create(Ram_quota *q, L4_msg_tag t, Utcb const *u, int *err)
{
  static_assert(UTCB_AREA_MR == 0 || UTCB_AREA_MR >= 2,
                "invalid value for UTCB_AREA_MR");
  if (UTCB_AREA_MR >= 2 && EXPECT_FALSE(t.words() <= UTCB_AREA_MR))
    {
      *err = L4_err::EInval;
      return 0;
    }

  using Alloc = Kmem_slab_t<TASK_TYPE>;

  *err = L4_err::ENomem;
  cxx::unique_ptr<TASK_TYPE> v(Alloc::q_new(q, q));

  if (EXPECT_FALSE(!v))
    return 0;

  if (EXPECT_FALSE(!v->initialize()))
    return 0;

  if (MUST_SYNC_KERNEL && (v->sync_kernel() < 0))
    return 0;

  if (UTCB_AREA_MR >= 2)
    {
      L4_fpage utcb_area(access_once(&u->values[UTCB_AREA_MR]));
      if (utcb_area.is_valid())
        {
          int e = v->alloc_ku_mem(utcb_area, false);
          if (e < 0)
            {
              *err = -e;
              return 0;
            }
        }
    }

  return v.release();
}

template<typename TASK_TYPE, bool MUST_SYNC_KERNEL, int UTCB_AREA_MR>
Kobject_iface * FIASCO_FLATTEN
Task::generic_factory(Ram_quota *q, Space *, L4_msg_tag t, Utcb const *u, int *err)
{
  return create<TASK_TYPE, MUST_SYNC_KERNEL, UTCB_AREA_MR>(q, t, u, err);
}

