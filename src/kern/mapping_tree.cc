#include "mapping_tree.h"

#include "kmem_slab.h"
#include "ram_quota.h"
#include "space.h"
#include "std_macros.h"


// Helpers

static Kmem_slab_t<Mapping> _mapping_allocator("Mapping");

//
// class Mapping_tree
//

void
Mapping_tree::erase(Space *owner)
{
  if (!front())
    return;

  Ram_quota *q = quota(owner);
  for (Iterator d = begin(); *d;)
    {
      // space is nullptr if the Mapping references a submap and
      // in this case trhe predecessor is the parent mapping that
      // contains the space pointer for this submap, so just use
      // the quota from the previous iteration.
      if (d->space())
        q = quota(d->space());

      assert(q);

      Mapping *m = *d;
      d = Mappings::erase(d);
      _mapping_allocator.q_del(q, m);
    }
}

Mapping_tree::Iterator
Mapping_tree::allocate_submap(Ram_quota *payer, Iterator parent)
{
  Mapping *m = _mapping_allocator.q_new(payer);
  if (!m)
    return end();

  if (*parent)
    {
      insert(m, parent);
      return ++parent;
    }
  else
    {
      push_front(m);
      return begin();
    }
}

Mapping_tree::Iterator
Mapping_tree::allocate(Ram_quota *payer, Iterator parent)
{
  if (*parent && parent->has_max_depth())
    return end();

  Mapping *m = _mapping_allocator.q_new(payer);
  if (!m)
    return end();

  Iterator test = parent;
  if (*test)
    {
      m->set_depth(parent->depth() + 1);
      ++test;
    }
  else
    {
      m->set_depth(0);
      test = begin();
    }


  if (*test && test->submap())
    parent = test;

  if (*parent)
    {
      insert(m, parent);
      return ++parent;
    }
  else
    {
      push_front(m);
      return begin();
    }
}

Mapping_tree::Iterator
Mapping_tree::free_mapping(Ram_quota *q, Iterator m)
{
  auto d = *m;
  m = Mappings::erase(m);
  _mapping_allocator.q_del(q, d);
  return m;
}


