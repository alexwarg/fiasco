#pragma once

#include <globalconfig.h>

#ifdef CONFIG_JDB

#include <jdb_trace.h>
#include <thread.h>
#include <cpu_lock.h>
#include "jdb_tbuf.h"
#include <types.h>

namespace Log
{

/** Page-fault logging.
 */
static void
do_page_fault_log(Address pfa, unsigned error_code, unsigned long eip)
{
  if (Jdb_pf_trace::check_restriction(current_thread()->dbg_id(), pfa))
    {
      auto guard = lock_guard(cpu_lock);

      Tb_entry_pf _local;
      Tb_entry_pf *tb = static_cast<Tb_entry_pf*>
	(EXPECT_TRUE(Jdb_pf_trace::log_buf()) ? Jdb_tbuf::new_entry()
				    : &_local);
      tb->set(current(), eip, pfa, error_code, current()->space());

      if (EXPECT_TRUE(Jdb_pf_trace::log_buf()))
	Jdb_tbuf::commit_entry(tb);
      else
	Jdb_tbuf::direct_log_entry(tb, "PF");
    }
}

/** Page-fault logging.
 */
inline void
page_fault(Address pfa, unsigned error_code, unsigned long eip)
{
  if (EXPECT_TRUE(!Jdb_pf_trace::log()))
    return;

  do_page_fault_log(pfa, error_code, eip);
}

}

#else // CONFIG_JDB

namespace Log
{
/** Page-fault logging.
 */
inline void
page_fault(Address, unsigned, unsigned long)
{}

}

#endif // CONFIG_JDB

