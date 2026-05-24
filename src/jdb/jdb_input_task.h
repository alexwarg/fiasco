
#pragma once

#include "jdb_module.h"
#include "jdb_types.h"
#include "types.h"
#include "l4_types.h"


class Task;
class Space;
class Kobject;

class Jdb_input_task_addr
{
public:
  static char     first_char;
  static char     first_char_have_task;

  static Task *task()
  { return cxx::dyn_cast<Task *>(_task); }

  static Space *space()
  { return _space; }

  static Address addr()
  { return _addr; }

  static Jdb_address address();
  static Jdb_module::Action_code
  action(void *&args, char const *&fmt, int &next_char);

private:
  static Kobject *_task;
  static Space   *_space;
  static Address  _addr;
};

