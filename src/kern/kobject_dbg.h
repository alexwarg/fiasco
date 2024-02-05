#pragma once

#include "config.h"

#if defined (CONFIG_JDB)

#include "spin_lock.h"
#include "lock_guard.h"
#include <cxx/dlist>
#include <cxx/hlist>
#include <cxx/dyn_cast>

struct Kobject_typeinfo_name
{
  cxx::Type_info const *type;
  char const *name;
};

#define JDB_DEFINE_TYPENAME(type, name) \
  static __attribute__((used, section(".debug.jdb.typeinfo_table"))) \
  Kobject_typeinfo_name const typeinfo_name__ ## type ## __entry =   \
    { cxx::Typeid<type>::get(), name }

class Kobject_dbg : public cxx::D_list_item
{
  friend class Jdb_kobject;
  friend class Jdb_kobject_list;
  friend class Jdb_mapdb;

public:
  class Dbg_extension : public cxx::H_list_item
  {
  public:
    virtual ~Dbg_extension() = 0;
  };

public:
  typedef cxx::H_list<Dbg_extension> Dbg_ext_list;
  Dbg_ext_list _jdb_data;

private:
  Mword _dbg_id;

public:
  Mword dbg_id() const { return _dbg_id; }

  virtual cxx::_dyn::Type _cxx_dyn_type() const = 0;
  virtual ~Kobject_dbg() = 0;


  typedef cxx::D_list<Kobject_dbg> Kobject_list;
  typedef Kobject_list::Iterator Iterator;
  typedef Kobject_list::Const_iterator Const_iterator;

  static Spin_lock<> _kobjects_lock;
  static Kobject_list _kobjects;

  static Iterator begin() { return _kobjects.begin(); }
  static Iterator end() { return _kobjects.end(); }

  static
  Iterator pointer_to_obj(void const *p)
  {
    for (Iterator l = _kobjects.begin(); l != _kobjects.end(); ++l)
      {
        auto ti = l->_cxx_dyn_type();
        Mword a = reinterpret_cast<Mword>(ti.base);
        if (a <= Mword(p) && Mword(p) < (a + ti.type->size))
          return l;
      }
    return _kobjects.end();
  }

  static
  unsigned long pointer_to_id(void const *p)
  {
    Iterator o = pointer_to_obj(p);
    if (o != _kobjects.end())
      return o->dbg_id();
    return ~0UL;
  }

  static
  bool is_kobj(void const *o)
  {
    return pointer_to_obj(o) != _kobjects.end();
  }

  static
  Iterator id_to_obj(unsigned long id)
  {
    for (Iterator l = _kobjects.begin(); l != _kobjects.end(); ++l)
      {
        if (l->dbg_id() == id)
          return l;
      }
    return end();
  }

  static
  unsigned long obj_to_id(void const *o)
  {
    return pointer_to_id(o);
  }

protected:
  Kobject_dbg()
  {
    auto guard = lock_guard(_kobjects_lock);

    _dbg_id = _next_dbg_id++;
    _kobjects.push_back(this);
  }


private:
  static unsigned long _next_dbg_id;
};

inline
Kobject_dbg::Dbg_extension::~Dbg_extension()
{}

inline
Kobject_dbg::~Kobject_dbg()
{
    {
      auto guard = lock_guard(_kobjects_lock);
      _kobjects.remove(this);
    }

  while (Dbg_extension *ex = _jdb_data.front())
    delete ex;
}


#else // CONFIG_JDB

#define JDB_DEFINE_TYPENAME(type, name)

class Kobject_dbg
{
public:
  typedef unsigned long Iterator;

  unsigned long dbg_id() const
  { return 0; }

  static
  unsigned long dbg_id(void const *)
  { return ~0UL; }

  static
  Iterator pointer_to_obj(void const *)
  { return 0; }


  static
  unsigned long pointer_to_id(void const *)
  { return ~0UL; }

  static
  bool is_kobj(void const *)
  { return false; }

  static
  Iterator id_to_obj(unsigned long)
  { return 0; }

  static
  unsigned long obj_to_id(void const *)
  { return ~0UL; }
};

#endif // CONFIG_JDB

