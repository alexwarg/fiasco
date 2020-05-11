#pragma once

#include "jdb_module.h"
#include "jdb_list.h"
#include "kobject.h"
#include "string_buffer.h"

#include <cxx/slist>

class Kobject;
class Jdb_kobject_handler;

class Jdb_kobject : public Jdb_module
{
public:
  typedef cxx::S_list_tail<Jdb_kobject_handler> Handler_list;
  typedef Handler_list::Const_iterator Handler_iter;

  Jdb_kobject();

  Handler_list handlers;
  Handler_list global_handlers;

  void register_handler(Jdb_kobject_handler *h);
  Jdb_kobject_handler *find_handler(Kobject_common *o);
  bool handle_obj(Kobject *o, int lvl);
  static char const *kobject_type(Kobject_common *o);
  static void obj_description(String_buffer *buffer, String_buffer *help_text,
                              bool dense, Kobject_dbg *o);

  Action_code action(int cmd, void *&, char const *&, int &) override;
  Cmd const *cmds() const override;
  int num_cmds() const override;

  static void init();
  static Jdb_kobject *module();
  static void print_uid(Kobject_common *o, int task_format = 0);


private:
  static void *kobjp;

  static void print_kobj(Kobject *o);
  static int fmt_handler(char /*fmt*/, int *size, char const *cmd_str, void *arg);
};


class Jdb_kobject_handler : public cxx::S_list_item
{
  friend class Jdb_kobject;

public:
  template<typename T>
  Jdb_kobject_handler(T const *) : kobj_type(cxx::Typeid<T>::get()) {}
  Jdb_kobject_handler() : kobj_type(0) {}
  cxx::Type_info const *kobj_type;
  virtual bool show_kobject(Kobject_common *o, int level) = 0;
  virtual void show_kobject_short(String_buffer *, Kobject_common *, bool) {}
  virtual Kobject_common *follow_link(Kobject_common *o) { return o; }
  virtual ~Jdb_kobject_handler() {}
  virtual bool invoke(Kobject_common *o, Syscall_frame *f, Utcb *utcb);
  virtual bool handle_key(Kobject_common *, int /*keycode*/) { return false; }
  virtual char const *help_text(Kobject_common *) const { return 0; };
  virtual Kobject *parent(Kobject_common *) { return 0; }
  char const *kobject_type(Kobject_common *o) const
  { return _kobject_type(o); }

  static char const *_kobject_type(Kobject_common *o)
  {
    extern Kobject_typeinfo_name const _jdb_typeinfo_table[];
    extern Kobject_typeinfo_name const _jdb_typeinfo_table_end[];

    for (Kobject_typeinfo_name const *t = _jdb_typeinfo_table;
        t != _jdb_typeinfo_table_end; ++t)
      if (t->type == cxx::dyn_typeid(o))
        return t->name;

    return "no type name";
  }

  bool is_global() const { return !kobj_type; }

protected:
  enum {
    Op_set_name         = 0,
    Op_global_id        = 1,
    Op_kobj_to_id       = 2,
    Op_query_log_typeid = 3,
    Op_switch_log       = 4,
    Op_get_name         = 5,
    Op_query_log_name   = 6,
  };
};

class Jdb_kobject_extension : public Kobject_dbg::Dbg_extension
{
public:
  virtual ~Jdb_kobject_extension() {}
  virtual char const *type() const = 0;

  template< typename T >
  static T *find_extension(Kobject_common const *o)
  {
    for (auto const &&ex: o->dbg_info()->_jdb_data)
      {
        if (!ex)
          return 0;

        Jdb_kobject_extension *je = static_cast<Jdb_kobject_extension*>(ex);
        if (je->type() == T::static_type)
          return static_cast<T*>(je);
      }

    return 0;
  }
};

class Jdb_kobject_list : public Jdb_list
{
public:
  typedef bool Filter_func(Kobject_common const *);

  Jdb_kobject_list();
  explicit Jdb_kobject_list(Filter_func *filt);

  void show_item(String_buffer *buffer, String_buffer *help_text,
                 void *item) const override;
  bool enter_item(void *item) const override;
  void *follow_link(void *item) override;
  bool handle_key(void *item, int keycode) override;
  int seek(int cnt, void **item) override;
  char const *show_head() const override;
  char const *get_mode_str() const override;
  void next_mode() override;
  void *get_valid(void *o) override;

  struct Mode : cxx::S_list_item
  {
    char const *name;
    Filter_func *filter;
    typedef cxx::S_list_bss<Mode> Mode_list;
    static Mode_list modes;

    Mode(char const *name, Filter_func *filter)
    : name(name), filter(filter)
    {
      // make sure that non-filtered mode is first in the list so that we
      // get this one displayed initially
      if (!filter)
        modes.push_front(this);
      else
        {
          Mode_list::Iterator i = modes.begin();
          if (i != modes.end())
            ++i;
          modes.insert_before(this, i);
        }
    }
  };

  void *get_head() const override
  { return Kobject::from_dbg(Kobject_dbg::begin()); }

private:
  Mode::Mode_list::Const_iterator _current_mode;
  Filter_func *_filter;

  void *get_first();
  Kobject *next(Kobject *obj);
  Kobject *prev(Kobject *obj);
};

