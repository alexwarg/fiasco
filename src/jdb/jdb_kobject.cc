
#include "jdb_kobject.h"
#include "jdb_module.h"
#include "jdb_list.h"
#include "kobject.h"
#include "string_buffer.h"

#include <cxx/slist>
#include <climits>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "entry_frame.h"
#include "jdb.h"
#include "jdb_core.h"
#include "jdb_module.h"
#include "jdb_screen.h"
#include "kernel_console.h"
#include "kobject.h"
#include "keycodes.h"
#include "ram_quota.h"
#include "simpleio.h"
#include "space.h"
#include "static_init.h"

Jdb_kobject_list::Mode::Mode_list Jdb_kobject_list::Mode::modes;

class Jdb_kobject_id_hdl : public Jdb_kobject_handler
{
public:
  virtual bool show_kobject(Kobject_common *, int) override { return false; }
  virtual ~Jdb_kobject_id_hdl() {}

  bool invoke(Kobject_common *o, Syscall_frame *f, Utcb *utcb) override
  {
    if (   utcb->values[0] != Op_global_id
        && utcb->values[0] != Op_kobj_to_id)
      return false;

    if (utcb->values[0] == Op_global_id)
      utcb->values[0] = o->dbg_info()->dbg_id();
    else
      utcb->values[0] = Kobject_dbg::pointer_to_id(reinterpret_cast<void *>(utcb->values[1]));
    f->tag(Kobject_iface::commit_result(0, 1));
    return true;
  }
};


void *
Jdb_kobject_list::get_first()
{
  Kobject_dbg::Iterator f = Kobject_dbg::begin();
  while (f != Kobject_dbg::end() && _filter && !_filter(Kobject::from_dbg(f)))
    ++f;
  return Kobject::from_dbg(f);
}

Jdb_kobject_list::Jdb_kobject_list(Filter_func *filt)
: Jdb_list(), _current_mode(Mode::modes.end()), _filter(filt)
{ set_start(get_first()); }

Jdb_kobject_list::Jdb_kobject_list()
: Jdb_list(), _current_mode(Mode::modes.begin())
{
  if (_current_mode != Mode::modes.end())
    _filter = _current_mode->filter;

  set_start(get_first());
}

void
Jdb_kobject_list::show_item(String_buffer *buffer, String_buffer *help_text,
                            void *item) const
{
  if (!item)
    return;
  Jdb_kobject::obj_description(buffer, help_text, false,
                               static_cast<Kobject*>(item)->dbg_info());
}

bool
Jdb_kobject_list::enter_item(void *item) const
{
  Kobject *o = static_cast<Kobject*>(item);
  return Jdb_kobject::module()->handle_obj(o, 1);
}

void *
Jdb_kobject_list::follow_link(void *item)
{
  Kobject *o = static_cast<Kobject*>(item);
  if (Jdb_kobject_handler *h = Jdb_kobject::module()->find_handler(o))
    return h->follow_link(o);

  return item;
}

bool
Jdb_kobject_list::handle_key(void *item, int keycode)
{
  Kobject *o = static_cast<Kobject*>(item);

  // in case of overlayprint
  Jdb::cursor(3, 1);

  bool handled = false;
  for (Jdb_kobject::Handler_iter h = Jdb_kobject::module()->global_handlers.begin();
       h != Jdb_kobject::module()->global_handlers.end(); ++h)
    handled |= h->handle_key(o, keycode);

  if (Jdb_kobject_handler *h = Jdb_kobject::module()->find_handler(o))
    handled |= h->handle_key(o, keycode);

  return handled;
}

inline
Kobject *
Jdb_kobject_list::next(Kobject *obj)
{
  if (!obj)
    return 0;

  Kobject_dbg::Iterator o = Kobject_dbg::Kobject_list::iter(obj->dbg_info());

  do
    {
      ++o;
      if (o == Kobject_dbg::end())
	return 0;
    }
  while (_filter && !_filter(Kobject::from_dbg(*o)));
  return Kobject::from_dbg(*o);
}

inline
Kobject *
Jdb_kobject_list::prev(Kobject *obj)
{
  if (!obj)
    return 0;

  Kobject_dbg::Iterator o = Kobject_dbg::Kobject_list::iter(obj->dbg_info());

  do
    {
      --o;
      if (o == Kobject_dbg::end())
	return 0;
    }
  while (_filter && !_filter(Kobject::from_dbg(*o)));
  return Kobject::from_dbg(*o);
}

int
Jdb_kobject_list::seek(int cnt, void **item)
{
  Kobject *c = static_cast<Kobject*>(*item);
  int i;
  if (cnt > 0)
    {
      for (i = 0; i < cnt; ++i)
	{
	  Kobject *n = next(c);
	  if (!n)
	    break;
	  c = n;
	}
    }
  else if (cnt < 0)
    {
      for (i = 0; i < -cnt; ++i)
	{
	  Kobject *n = prev(c);
	  if (!n)
	    break;
	  c = n;
	}
    }
  else
    return 0;

  if (*item != c)
    {
      *item = c;
      return i;
    }

  return 0;
}

char const *
Jdb_kobject_list::show_head() const
{
  return "[Objects]";
}


char const *
Jdb_kobject_list::get_mode_str() const
{
  if (_current_mode == Mode::modes.end())
    return "[Objects]";
  return _current_mode->name;
}



void
Jdb_kobject_list::next_mode()
{
  if (_current_mode == Mode::modes.end())
    return;

  ++_current_mode;
  if (_current_mode == Mode::modes.end())
    _current_mode = Mode::modes.begin();

  _filter = _current_mode->filter;
}

/* When the mode changes the current object may get invisible,
 * get a new visible one */
void *
Jdb_kobject_list::get_valid(void *o)
{
  if (!_filter)
    return o;

  if (_filter && _filter(static_cast<Kobject*>(o)))
    return o;
  return get_first();
}

bool
Jdb_kobject_handler::invoke(Kobject_common *, Syscall_frame *, Utcb *)
{ return false; }

char const *
Jdb_kobject_handler::_kobject_type(Kobject_common *o)
{
  extern Kobject_typeinfo_name const _jdb_typeinfo_table[];
  extern Kobject_typeinfo_name const _jdb_typeinfo_table_end[];

  for (auto const *t = _jdb_typeinfo_table; t != _jdb_typeinfo_table_end; ++t)
    if (t->type == cxx::dyn_typeid(o))
      return t->name;

  return JDB_ANSI_COLOR(lightred) "NO TYPE" JDB_ANSI_COLOR(default);
}

void *Jdb_kobject::kobjp;

Jdb_kobject::Jdb_kobject()
  : Jdb_module("INFO")
{}


void
Jdb_kobject::register_handler(Jdb_kobject_handler *h)
{
  if (h->is_global())
    global_handlers.push_back(h);
  else
    handlers.push_back(h);
}

Jdb_kobject_handler *
Jdb_kobject::find_handler(Kobject_common *o)
{
  for (Handler_iter h = handlers.begin(); h != handlers.end(); ++h)
    {
      auto r = o->_cxx_dyn_type();
      if (r.type == h->kobj_type)
        return *h;

      // XXX: may be we should sort the handlers: most derived first
      cxx::uintptr_t delta;
      if (r.type->do_cast(h->kobj_type, cxx::Typeid<Kobject_common>::get(),
                          reinterpret_cast<cxx::uintptr_t>(o)
                          - reinterpret_cast<cxx::uintptr_t>(r.base), &delta))
        return *h;
    }

  return 0;
}

bool
Jdb_kobject::handle_obj(Kobject *o, int lvl)
{
  if (Jdb_kobject_handler *h = find_handler(o))
    return h->show_kobject(o, lvl);

  return true;
}

char const *
Jdb_kobject::kobject_type(Kobject_common *o)
{
  if (Jdb_kobject_handler *h = module()->find_handler(o))
    return h->kobject_type(o);

  return Jdb_kobject_handler::_kobject_type(o);
}


void
Jdb_kobject::obj_description(String_buffer *buffer, String_buffer *help_text,
                             bool dense, Kobject_dbg *o)
{
  Kobject *k = Kobject::from_dbg(o);

  if (buffer)
    buffer->printf(dense ? "%lx %lx [%-*s]" : "%8lx %08lx [%-*s]", o->dbg_id(),
                   reinterpret_cast<Mword>(k), dense ? 0 : 7, kobject_type(k));

  char const *ht;

  for (Handler_iter h = module()->global_handlers.begin();
       h != module()->global_handlers.end(); ++h)
    {
      if (buffer)
        h->show_kobject_short(buffer, k, dense);

      if (help_text && (ht = h->help_text(k)))
        help_text->printf(" %s", ht);
    }

  if (Jdb_kobject_handler *oh = Jdb_kobject::module()->find_handler(k))
    {
      if (buffer)
        oh->show_kobject_short(buffer, k, dense);

      if (help_text && (ht = oh->help_text(k)))
        help_text->printf(" %s", ht);
    }
}

Jdb_module::Action_code
Jdb_kobject::action(int cmd, void *&, char const *&, int &)
{
  if (cmd == 0)
    {
      puts("");
      Kobject_dbg::Iterator i = Kobject_dbg::pointer_to_obj(kobjp);
      if (i == Kobject_dbg::end())
	printf("Not a kobj.\n");
      else
        {
          Kobject *k = Kobject::from_dbg(i);
          if (!handle_obj(k, 0))
            printf("Kobj w/o handler: ");
          puts("");
        }
      return NOTHING;
    }
  else if (cmd == 1)
    {
      Jdb_kobject_list list;
      list.do_list();
    }
  return NOTHING;
}

Jdb_module::Cmd const *
Jdb_kobject::cmds() const
{
  static Cmd cs[] =
    {
	{ 0, "K", "kobj", "%p", "K<kobj_ptr>\tshow information for kernel object", 
	  &kobjp },
	{ 1, "Q", "listkobj", "", "Q\tshow information for kernel objects", 0 },
    };
  return cs;
}

int
Jdb_kobject::num_cmds() const
{ return 2; }

STATIC_INITIALIZE_P(Jdb_kobject, JDB_MODULE_INIT_PRIO);

int
Jdb_kobject::fmt_handler(char /*fmt*/, int *size, char const *cmd_str, void *arg)
{
  char buffer[20];

  int pos = 0;
  int c;
  Address n;

  *size = sizeof(void*);

  while((c = Jdb_core::cmd_getchar(cmd_str)) != ' ' && c != KEY_RETURN
        && c != KEY_RETURN_2)
    {
      if(c==KEY_ESC)
	return 3;

      if((c==KEY_BACKSPACE || c==KEY_BACKSPACE_2) && pos>0)
	{
	  putstr("\b \b");
	  --pos;
          continue;
	}

      if (pos < int{sizeof(buffer)} - 1
          && (   (c >= '0' && c <= '9')
              || (c >= 'a' && c <= 'f')
              || (c >= 'A' && c <= 'F')
              || (c == 'P' && pos == 0)))
	{
          Jdb_core::cmd_putchar(c);
	  buffer[pos++] = c;
	  buffer[pos] = 0;
	}
    }

  Kobject **a = static_cast<Kobject**>(arg);

  if (!pos)
    {
      *a = 0;
      return 0;
    }

  char const *num = buffer;
  if (buffer[0] == 'P')
    num = buffer + 1;

  n = strtoul(num, 0, 16);

  Kobject_dbg::Iterator ko;

  if (buffer[0] != 'P')
    ko = Kobject_dbg::id_to_obj(n);
  else
    ko = Kobject_dbg::pointer_to_obj(reinterpret_cast<void*>(n));

  if (ko != Kobject_dbg::end())
    *a = Kobject::from_dbg(ko);
  else
    *a = 0;

  return 0;
}

void
Jdb_kobject::init()
{
  module();

  Jdb_core::add_fmt_handler('q', fmt_handler);

//  static Jdb_handler enter(at_jdb_enter);

  static Jdb_kobject_id_hdl id_hdl;
  module()->register_handler(&id_hdl);
}

Jdb_kobject *
Jdb_kobject::module()
{
  static Jdb_kobject jdb_kobj_module;
  return &jdb_kobj_module;
}

// Be robust if this object is invalid
void
Jdb_kobject::print_uid(Kobject_common *o, int task_format)
{
  if (!o)
    {
      printf("%*.s", task_format, "---");
      return;
    }

  if (Kobject_dbg::is_kobj(o))
    {
      printf("%*.lx", task_format, o->dbg_info()->dbg_id());
      return;
    }

  printf("\033[31;1m%*s%p\033[m", task_format, "???", static_cast<void *>(o));
  return;
}

// used from ASM
extern "C" void
sys_invoke_debug(Kobject_iface *o, Syscall_frame *f);

extern "C" void
sys_invoke_debug(Kobject_iface *o, Syscall_frame *f)
{
  if (!o)
    {
      f->tag(Kobject_iface::commit_result(-L4_err::EInval));
      return;
    }

  Utcb *utcb = current_thread()->utcb().access();
  //printf("sys_invoke_debug: [%p] -> %p\n", o, f);
  Jdb_kobject_handler *h = Jdb_kobject::module()->find_handler(o);
  if (h && h->invoke(o, f, utcb))
    return;

  for (Jdb_kobject::Handler_iter i = Jdb_kobject::module()->global_handlers.begin();
       i != Jdb_kobject::module()->global_handlers.end(); ++i)
    if (i->invoke(o, f, utcb))
      return;

  f->tag(Kobject_iface::commit_result(-L4_err::ENosys));
}

static Jdb_kobject_list::Mode INIT_PRIORITY(JDB_MODULE_INIT_PRIO) all("[ALL]", 0);

