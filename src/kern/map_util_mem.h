#pragma once

#include <l4_types.h>
#include <l4_error.h>
#include <l4_msg_item.h>
#include <space.h>

L4_error __attribute__((nonnull(1, 3)))
mem_map(Space *from, L4_fpage fp_from,
        Space *to, L4_fpage fp_to, L4_msg_item control);

L4_fpage::Rights __attribute__((nonnull(1)))
mem_fpage_unmap(Space *space, L4_fpage fp, L4_map_mask mask);

void
init_mapdb_mem(Space *sigma0);

