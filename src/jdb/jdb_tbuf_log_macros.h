
#pragma once

#include <globalconfig.h>

#ifdef CONFIG_JDB_LOGGING

#include <jdb_tbuf_log_macros_arch.h>

#define END_LOG_EVENT						\
	}							\
    } while (0)

#else // ! CONFIG_JDB_LOGGING

#define BEGIN_LOG_EVENT(name, sc, fmt)				\
  if (0)							\
    { char __do_log__ = 0; (void)__do_log__;

#define END_LOG_EVENT						\
    }

#endif // ! CONFIG_JDB_LOGGING



