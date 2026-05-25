#pragma once

#include "config.h"
#include "jdb_kobject.h"
#include "l4_types.h"
#include "initcalls.h"


class Jdb_kobject_name : public Jdb_kobject_extension
{
public:
  static char const *const static_type;
  virtual char const *type() const override { return static_type; }

  ~Jdb_kobject_name() {}

  void *operator new (size_t) throw();
  void operator delete (void *);

  int max_len() { return sizeof(_name); }

  Jdb_kobject_name() { _name[0] = 0; }

  void name(char const *name, int size);

  inline char const *name() const { return _name; }
  inline char       *name()       { return _name; }

  static FIASCO_INIT void init();

private:
  char _name[16];

  static Jdb_kobject_name *_names;
};
