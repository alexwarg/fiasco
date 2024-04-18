#pragma once

#include <globalconfig.h>
#include <l4_types.h>
#include <l4_msg_item.h>

#if ! defined(CONFIG_IA32) && ! defined (CONFIG_AMD64)

class Space;

// Empty dummy functions when I/O protection is disabled
[[gnu::nonnull(1)]]
inline
void init_mapdb_io(Space *)
{}

[[gnu::nonnull(1, 3)]]
inline
L4_error
io_map(Space *, L4_fpage const &, Space *, L4_fpage const &, L4_msg_item)
{
  return L4_error::None;
}

[[gnu::nonnull(1)]]
inline
L4_fpage::Rights
io_fpage_unmap(Space * /*space*/, L4_fpage const &/*fp*/, L4_map_mask)
{
  return L4_fpage::Rights(0);
}

#else

class Space;

[[gnu::nonnull(1)]]
void init_mapdb_io(Space *space);

[[gnu::nonnull(1, 3)]]
L4_error
io_map(Space *from, L4_fpage fp_from,
       Space *to,   L4_fpage fp_to,
       L4_msg_item control);

[[gnu::nonnull(1)]]
L4_fpage::Rights
io_fpage_unmap(Space *space, L4_fpage fp, L4_map_mask mask);

#endif
