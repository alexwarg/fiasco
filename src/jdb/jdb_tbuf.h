#pragma once

#include <cxx/atomic>

#include "globalconfig.h"
#include "jdb_ktrace.h"
#include "l4_types.h"
#include "std_macros.h"
#include "tb_entry.h"
#include "spin_lock.h"
#include <jdb_tbuf_log_macros.h>

class Context;
class Log_event;
struct Tracebuffer_status;

class Jdb_tbuf
{
public:
  static void (*direct_log_entry)(Tb_entry*, const char*);

  enum
  {
    Event  = 1,
    Result = 2
  };

  static unsigned char get_entry_status(Tb_log_table_entry const *e);
  static void set_entry_status(Tb_log_table_entry const *e,
                               unsigned char value);

  static Tracebuffer_status *status() { return _status; }
  static Address size() { return _size; }

  /** Clear tracebuffer. */
  static void clear_tbuf();
  /** Return pointer to new tracebuffer entry. */
  static Tb_entry *new_entry();
  static Mword entries();
  /** Tb_entry => tracebuffer index. */
  static Mword idx(Tb_entry const *e);
  /** Event number => Tb_entry. */

  /** Event number => tracebuffer index.
   * @param  nr  number of event
   * @return tracebuffer index of event which has the number nr or
   *         -1 if there is no event with this number or
   *         -2 if the event is currently hidden. */
  static Mword search_to_idx(Mword nr);

  /** Return pointer to tracebuffer event.
   * Don't count hidden events.
   * @param  position of event in tracebuffer:
   *         0 is last event, 1 the event before and so on
   * @return pointer to tracebuffer event
   *
   * event with idx == 0 is the last event queued in
   * event with idx == 1 is the event before */
  static Tb_entry *lookup(Mword look_idx);

  /** Return some information about log event.
   * @param idx             index of event to determine the info
   *                        (0 = most recent event, 1 = penultimate event, ...)
   * @param[out] number     event number
   * @param[out] klock      event value of kernel clock
   * @param[out] tsc        event value of CPU cycles
   * @param[out] pmc1       event value of perf counter 1 cycles
   * @param[out] pmc2       event value of perf counter 2 cycles
   * @param[out] committed  false: event not yet committed
   * @return 0 if something wrong, 1 if everything OK */
  static int event(Mword idx, Mword *number, Unsigned32 *kclock,
                   Unsigned64 *tsc, Unsigned32 *pmc1, Unsigned32 *pmc2,
                   bool *committed = nullptr);

  /** Get difference CPU cycles between event idx and event idx+1 on the same CPU.
   * @param idx position of first event in tracebuffer
   * @retval difference in CPU cycles
   * @return 0 if something wrong, 1 if everything ok */
  static int diff_tsc(Mword idx, Signed64 *delta);

  /** Get difference perfcnt cycles between event idx and event idx+1.
   * @param idx position of first event in tracebuffer
   * @param nr  number of perfcounter (0=first, 1=second)
   * @retval difference in perfcnt cycles
   * @return 0 if something wrong, 1 if everything ok */
  static int diff_pmc(Mword idx, Mword nr, Signed32 *delta);

  template<typename T>
  static inline T *new_entry()
  {
    static_assert(sizeof(T) <= sizeof(Tb_entry_union), "tb entry T too big");
    return static_cast<T*>(new_entry());
  }

  /** Commit tracebuffer entry.
   * This function is executed when the entry is complete. At the moment it
   * does nothing. */
  static void commit_entry(Tb_entry *tb)
  {
    tb->number(tb->number() - 1000);
  }

  /** Return number of entries currently allocated in tracebuffer.
   * @return number of entries */
  static Mword unfiltered_entries()
  {
    return _number.load() > _max_entries ? _max_entries : _number.load();
  }

  /** Return maximum number of entries in tracebuffer.
   * @return number of entries */
  static Mword max_entries()
  {
    return _max_entries;
  }

  /** Check if event is valid.
   * @param idx position of event in tracebuffer
   * @return 0 if not valid, 1 if valid */
  static int event_valid(Mword idx)
  {
    return idx < unfiltered_entries();
  }

  /** Return pointer to tracebuffer event.
   * @param  position of event in tracebuffer:
   *         0 is last event, 1 the event before and so on
   * @return pointer to tracebuffer event
   *
   * event with idx == 0 is the last event queued in
   * event with idx == 1 is the event before */
  static Tb_entry *unfiltered_lookup(Mword idx)
  {
    if (!event_valid(idx))
      return 0;

    return buffer() + ((_number.load() - idx - 1) & (_max_entries - 1));
  }

  static Mword unfiltered_idx(Tb_entry const *e)
  {
    auto *ef = static_cast<Tb_entry_union const *>(e);
    return (_number.load() - (ef - buffer()) - 1) & (_max_entries - 1);
  }

  static Tb_entry *search(Mword nr)
  {
    Tb_entry *e;

    for (Mword idx = 0; (e = unfiltered_lookup(idx)); idx++)
      if (e->number() == nr)
        return e;

    return 0;
  }

  static void enable_filter()
  {
    _filter_enabled = 1;
  }

  static void disable_filter()
  {
    _filter_enabled = 0;
  }

protected:
  static Tb_entry_union *buffer() { return _buffer; }

  static Mword		_max_entries;	// maximum number of entries
  static Mword          _filter_enabled;// !=0 if filter is active
  static cxx::atomic<Mword> _number;	// current event number
  static Address        _size;		// size of memory area for tbuffer
  static Tracebuffer_status *_status;
  static Tb_entry_union *_buffer;
};

