
#include "jdb_trace.h"

#include <cstdio>

#include "config.h"
#include "jdb_ktrace.h"
#include "jdb_tbuf.h"
#include "simpleio.h"

int         Jdb_ipc_trace::_other_thread;
Mword       Jdb_ipc_trace::_gthread;
int         Jdb_ipc_trace::_other_task;
Mword       Jdb_ipc_trace::_task;
int         Jdb_ipc_trace::_snd_only;
int         Jdb_ipc_trace::_log;
int         Jdb_ipc_trace::_log_to_buf;
int         Jdb_ipc_trace::_log_result;
int         Jdb_ipc_trace::_slow_ipc;

int         Jdb_pf_trace::_other_thread;
Mword       Jdb_pf_trace::_gthread;
Addr_range  Jdb_pf_trace::_addr;
int         Jdb_pf_trace::_log;
int         Jdb_pf_trace::_log_to_buf;


void
Jdb_ipc_trace::clear_restriction()
{
  _other_thread = 0;
  _gthread      = 0;
  _other_task   = 0;
  _task         = 0;
  _snd_only     = 0;
}

void
Jdb_ipc_trace::show()
{
  if (_log)
    {
      printf("IPC logging%s%s enabled%s",
          _log_result ? " incl. results" : "",
          _log_to_buf ? " to tracebuffer" : "",
          _log_to_buf ? "" : " (exit with 'i', proceed with other key)");
      if (_gthread != 0)
        {
          printf("\n    restricted to thread%s %lx%s",
                 _other_thread ? "s !=" : "",
                 _gthread,
                 _snd_only ? ", snd-only" : "");
        }
      if (_task != 0)
        {
          printf("\n    restricted to task%s %lx",
              _other_task ? "s !=" : "", _task);
        }
    }
  else
    {
      printf("IPC logging disabled -- using the IPC %s path",
          _slow_ipc
            ? "slow"
            : "C fast");
    }

  putchar('\n');
}

void
Jdb_pf_trace::show()
{
  if (_log)
    {
      int res_enabled = 0;
      BEGIN_LOG_EVENT("Page fault results", "pfr", Tb_entry_pf)
      res_enabled = 1;
      END_LOG_EVENT;
      printf("PF logging%s%s enabled",
             res_enabled ? " incl. results" : "",
             _log_to_buf ? " to tracebuffer" : "");
      if (_gthread != 0)
        {
          printf(", restricted to thread%s %lx",
                 _other_thread ? "s !=" : "",
                 _gthread);
        }
      if (_addr.lo || _addr.hi)
        {
          if (_gthread != 0)
            putstr(" and ");
          else
            putstr(", restricted to ");
          if (_addr.lo <= _addr.hi)
            printf(L4_PTR_FMT " <= pfa <= " L4_PTR_FMT
                   , _addr.lo, _addr.hi);
          else
            printf("pfa < " L4_PTR_FMT " || pfa > " L4_PTR_FMT,
                   _addr.hi, _addr.lo);
        }
    }
  else
    putstr("PF logging disabled");
  putchar('\n');
}

void
Jdb_pf_trace::clear_restriction()
{
  _other_thread = 0;
  _gthread      = 0;
  _addr.lo      = 0;
  _addr.hi      = 0;
}
