#pragma once

#include "l4_types.h"
#include "entry_frame.h"

class Syscall_frame;
typedef struct
{
  Address lo, hi;
} Addr_range;

class Jdb_ipc_trace
{
public:
  static int         _other_thread;
  static Mword       _gthread;
  static int         _other_task;
  static Mword       _task;
  static int         _snd_only;
  static int         _log;
  static int         _log_to_buf;
  static int         _log_result;
  static int         _slow_ipc;
  friend class Jdb_set_trace;

  static inline int log()        { return _log; }
  static inline int log_buf()    { return _log_to_buf; }
  static inline int log_result() { return _log_result; }

  static inline int check_restriction(Mword id, Mword task,
                                       Syscall_frame *ipc_regs, Mword dst_task)
  {
    return (   ((_gthread == 0)
                || ((_other_thread) ^ (_gthread == id))
                )
             && ((!_snd_only || ipc_regs->ref().valid()))
             && ((_task == 0)
                || ((_other_task)
                    ^ ((_task == task) || (_task == dst_task))))
            );
  }

  static void clear_restriction();
  static void show();
};

class Jdb_pf_trace
{
public:
  static inline int log()     { return _log; }
  static inline int log_buf() { return _log_to_buf; }

  static inline int check_restriction(Mword id, Address pfa)
  {
    return (   (((_gthread == 0)
                || ((_other_thread) ^ (_gthread == id))))
             && (!(_addr.lo | _addr.hi)
                || (_addr.lo <= _addr.hi && pfa >= _addr.lo && pfa <= _addr.hi)
                || (_addr.lo >  _addr.hi && pfa <  _addr.hi && pfa >  _addr.lo)));
  }

  static void clear_restriction();
  static void show();

private:
  static int         _other_thread;
  static Mword       _gthread;
  static Addr_range  _addr;
  static int         _log;
  static int         _log_to_buf;
  friend class Jdb_set_trace;
};
