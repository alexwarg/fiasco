
#include <jdb_tbuf.h>

#include "config.h"
#include "cpu_lock.h"
#include "initcalls.h"
#include "lock_guard.h"
#include "mem_unit.h"
#include "mem.h"
#include "std_macros.h"
#include <jdb_tbuf_log_entry_impl.h>

// read only: initialized at boot
Tracebuffer_status *Jdb_tbuf::_status;
Tb_entry_union *Jdb_tbuf::_buffer;
Address Jdb_tbuf::_size;
Mword Jdb_tbuf::_max_entries;

// read mostly (only modified in JDB)
Mword Jdb_tbuf::_filter_enabled;

// modified often (for each new entry)
cxx::atomic<Mword> Jdb_tbuf::_number;


static void direct_log_dummy(Tb_entry*, const char*)
{}

void (*Jdb_tbuf::direct_log_entry)(Tb_entry*, const char*) = &direct_log_dummy;

/** Clear tracebuffer. */
void
Jdb_tbuf::clear_tbuf()
{
  Mword i;

  for (i = 0; i < _max_entries; i++)
    buffer()[i].clear();

  _number.store(0);
}

/** Return pointer to new tracebuffer entry. */
Tb_entry*
Jdb_tbuf::new_entry()
{
  Mword n = _number.fetch_add(1);

  Tb_entry_union *tb = buffer() + (n & (_max_entries - 1));
  // As long as not all information is written, write an invalid number which
  // can be easily corrected in commit_entry(). See the 'committed' parameter
  // in  Jdb_tbuf::event(). There is still a short race between setting _number
  // and setting the entry's number field. Let's ignore that for now.
  tb->number(n + 1000);
  Mem::barrier();

  tb->rdtsc();
  tb->rdpmc1();
  tb->rdpmc2();

  return tb;
}


Mword
Jdb_tbuf::entries()
{
  if (!_filter_enabled)
    return unfiltered_entries();

  Mword cnt = 0;

  for (Mword idx = 0; idx<unfiltered_entries(); idx++)
    if (!buffer()[idx].is_hidden())
      cnt++;

  return cnt;
}

Tb_entry*
Jdb_tbuf::lookup(Mword look_idx)
{
  if (!_filter_enabled)
    return unfiltered_lookup(look_idx);

  for (Mword idx = 0;; idx++)
    {
      Tb_entry *e = unfiltered_lookup(idx);

      if (!e)
	return 0;
      if (e->is_hidden())
	continue;
      if (!look_idx--)
	return e;
    }
}

Mword
Jdb_tbuf::idx(Tb_entry const *e)
{
  if (!_filter_enabled)
    return unfiltered_idx(e);

  Tb_entry_union const *ef_next = buffer() + (_number.load() & (_max_entries - 1));
  Tb_entry_union const *ef = static_cast<Tb_entry_union const*>(e);
  Mword idx = static_cast<Mword>(-1);

  for (;;)
    {
      if (!ef->is_hidden())
        idx++;
      ef++;
      if (ef >= buffer() + _max_entries)
        ef -= _max_entries;
      if (ef == ef_next)
        break;
    }

  return idx;
}

Mword
Jdb_tbuf::search_to_idx(Mword nr)
{
  if (nr == static_cast<Mword>(-1))
    return static_cast<Mword>(-1);

  Tb_entry *e;

  if (!_filter_enabled)
    {
      e = search(nr);
      if (!e)
        return static_cast<Mword>(-1);
      return unfiltered_idx(e);
    }

  for (Mword idx_u = 0, idx_f = 0; (e = unfiltered_lookup(idx_u)); idx_u++)
    {
      if (e->number() == nr)
        return e->is_hidden() ? static_cast<Mword>(-2) : idx_f;

      if (!e->is_hidden())
        idx_f++;
    }

  return static_cast<Mword>(-1);
}

int
Jdb_tbuf::event(Mword idx, Mword *number, Unsigned32 *kclock,
		Unsigned64 *tsc, Unsigned32 *pmc1, Unsigned32 *pmc2,
		bool *committed)
{
  Tb_entry *e = lookup(idx);

  if (!e)
    return false;

  *number = e->number();
  if (kclock)
    *kclock = e->kclock();
  if (tsc)
    *tsc = e->tsc();
  if (pmc1)
    *pmc1 = e->pmc1();
  if (pmc2)
    *pmc2 = e->pmc2();
  if (committed)
    *committed = (e->number() < _number.load());
  return true;
}

int
Jdb_tbuf::diff_tsc(Mword idx, Signed64 *delta)
{
  Tb_entry *e      = lookup(idx);
  Tb_entry *e_prev;

  if (!e)
    return false;

  do
    {
      e_prev = lookup(++idx);
      if (!e_prev)
        return false;
    }
  while (e->cpu() != e_prev->cpu());

  *delta = e->tsc() - e_prev->tsc();
  return true;
}

int
Jdb_tbuf::diff_pmc(Mword idx, Mword nr, Signed32 *delta)
{
  Tb_entry *e      = lookup(idx);
  Tb_entry *e_prev = lookup(idx + 1);

  if (!e || !e_prev)
    return false;

  switch (nr)
    {
    case 0: *delta = e->pmc1() - e_prev->pmc1(); break;
    case 1: *delta = e->pmc2() - e_prev->pmc2(); break;
    }

  return true;
}
