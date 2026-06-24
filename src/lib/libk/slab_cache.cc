#include "slab_cache.h"

#include <lock_guard.h>
#include <cstdio>

void *
Slab_cache::alloc()	// request initialized member from cache
{
  void *unused_block = nullptr;
  void *ret;
    {
      auto guard = lock_guard(lock);

      Slab *s = get_available_locked();

      if (EXPECT_FALSE(!s))
        {
          guard.reset();

          char *m = static_cast<char*>(block_alloc(_slab_size, _slab_size));
          Slab *new_slab = nullptr;
          if (m)
            new_slab = new (m + _slab_size - sizeof(Slab)) Slab(_elem_num, _entry_size, m);

          guard.lock(&lock);

          // retry gettin a slab that might be allocated by a different
          // CPU meanwhile
          s = get_available_locked();

          if (!s)
            {
              // real OOM
              if (!m)
                return nullptr;

              _partial.add(new_slab);
              s = new_slab;
            }
          else
            unused_block = m;
        }

      ret = s->alloc();
      assert(ret);

      if (s->is_full())
        {
          cxx::H_list<Slab>::remove(s);
          _full.add(s);
        }
    }

  if (unused_block)
    block_free(unused_block, _slab_size);

  return ret;
}

void
Slab_cache::free(void *cache_entry) // return initialized member to cache
{
  Slab *to_free = nullptr;
    {
      auto guard = lock_guard(lock);

      Slab *s = reinterpret_cast<Slab*>
        ((reinterpret_cast<unsigned long>(cache_entry) & ~(_slab_size - 1)) + _slab_size - sizeof(Slab));

      bool was_full = s->is_full();

      s->free(cache_entry);

      if (was_full)
        {
          cxx::H_list<Slab>::remove(s);
          _partial.add(s);
        }
      else if (s->is_empty())
        {
          cxx::H_list<Slab>::remove(s);
          if (_num_empty < 2)
            {
              _empty.add(s);
              ++_num_empty;
            }
          else
            to_free = s;
        }
    }

  if (to_free)
    {
      to_free->~Slab();
      block_free(reinterpret_cast<char *>(to_free + 1) - _slab_size, _slab_size);
    }
}

unsigned long
Slab_cache::reap()		// request that cache returns memory to system
{
  Slab *s = nullptr;
  unsigned long sz = 0;

  for (;;)
    {
        {
          auto guard = lock_guard(lock);

          s = _empty.front();
          // nothing to free
          if (!s)
            break;

          cxx::H_list<Slab>::remove(s);
        }

      // explicitly call destructor to delete s;
      s->~Slab();
      block_free(reinterpret_cast<char *>(s + 1) - _slab_size, _slab_size);
      sz += _slab_size;
    }

  return sz;
}

// Debugging output
void
Slab_cache::debug_dump() const
{
  printf ("%s: %lu-KB slabs (elems per slab=%u ",
          _name, _slab_size / 1024, _elem_num);

  unsigned count = 0, total = 0, total_elems = 0;
  for (Slab_list::Const_iterator s = _full.begin(); s != _full.end(); ++s)
    {
      if (!s->is_full())
        printf ("\n*** wrongly-enqueued full slab found\n");

      ++count;
      total_elems += s->in_use();
    }

  total += count;

  printf ("%u full, ", count);

  count = 0;
  for (Slab_list::Const_iterator s = _partial.begin(); s != _partial.end(); ++s)
    {
      if (s->is_full() || s->is_empty())
        printf ("\n*** wrongly-enqueued full slab found\n");

      count++;
      total_elems += s->in_use();
    }

  total += count;

  printf ("%u used, ", count);

  count = 0;
  for (Slab_list::Const_iterator s = _empty.begin(); s != _empty.end(); ++s)
    {
      if (! s->is_empty())
        printf ("\n*** wrongly-enqueued nonempty slab found\n");
      count++;
    }

  unsigned total_used = total;
  total += count;

  printf ("%u empty = %u total) = %lu KB,\n  %u elems (size=%u)",
          count, total, total * _slab_size / 1024,
          total_elems, _entry_size);

  if (total_elems)
    printf (", overhead = %lu B (%lu B)  = %lu%% (%lu%%) \n",
            total * _slab_size - total_elems * _entry_size,
            total_used * _slab_size - total_elems * _entry_size,
            100 - total_elems * _entry_size * 100U / (total * _slab_size),
            100 - total_elems * _entry_size * 100U / (total_used * _slab_size));
  else
    printf ("\n");
}

