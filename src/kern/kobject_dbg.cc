#include "kobject_dbg.h"
#include "static_init.h"

Spin_lock<> Kobject_dbg::_kobjects_lock;
Kobject_dbg::Kobject_list Kobject_dbg::_kobjects INIT_PRIORITY(BOOTSTRAP_INIT_PRIO);
unsigned long Kobject_dbg::_next_dbg_id;


