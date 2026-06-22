#include "kobject.h"

void
Kobject::initiate_deletion(Kobject ***reap_list)
{
  existence_lock.invalidate();

  _next_to_reap = nullptr;
  **reap_list = this;
  *reap_list = &_next_to_reap;
}

