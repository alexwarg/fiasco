#pragma once

#include <l4_types.h>
#include <l4_error.h>
#include <l4_msg_item.h>
#include <space.h>

class Kobject;

L4_error
obj_map(Space *from, L4_fpage p_from,
        Space *to, L4_fpage fp_to, L4_msg_item control,
        Kobject ***reap_list);

L4_fpage::Rights __attribute__((nonnull(1)))
obj_fpage_unmap(Space * space, L4_fpage fp, L4_map_mask mask,
                Kobject ***reap_list);

L4_error
obj_map(Space *from, Cap_index snd_addr, unsigned long snd_size,
        Space *to, Cap_index rcv_addr,
        Kobject ***reap_list, bool grant = false,
        Obj_space::Attr attribs = Obj_space::Attr::Full());

bool
map(Kobject_iface *o, Obj_space* to, Space *to_id, Cap_index rcv_addr,
    Kobject ***reap_list);

