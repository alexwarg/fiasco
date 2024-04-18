#pragma once

#include <l4_types.h>
#include <l4_msg_item.h>
#include <kobject.h>

class Space;

L4_error
fpage_map(Space *from, L4_fpage fp_from, Space *to,
          L4_fpage fp_to, L4_msg_item control, Kobject::Reap_list *r);

L4_fpage::Rights
fpage_unmap(Space *space, L4_fpage fp, L4_map_mask mask, Kobject ***rl);

