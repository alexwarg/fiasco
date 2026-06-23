
#include <cassert>
#include <cstring>

#include <jdb_module.h>
#include "static_init.h"

Jdb_category::List Jdb_category::categories;
Jdb_module::List Jdb_module::modules;

static Jdb_category INIT_PRIORITY(JDB_CATEGORY_INIT_PRIO)
  misc_cat("MISC", "misc debugger commands", 2000);

Jdb_category::Jdb_category(char const *const name,
			   char const *const desc,
			   unsigned order)
  : _name(name), _desc(desc), _order(order)
{
  List::Iterator c = categories.begin();

  for (; c != categories.end(); ++c)
    if (c->_order >= order)
      break;
  categories.insert_before(this, c);
}

Jdb_category *Jdb_category::find(char const* name, bool _default)
{
  List::Const_iterator a;
  for (a = categories.begin();
       a != categories.end() && strcmp(a->name(), name) != 0; ++a)
    ;

  if (_default && a == categories.end())
    return &misc_cat;

  return *a;
}


Jdb_module::Jdb_module(char const *category)
  : _cat(Jdb_category::find(category, true))
{
  modules.push_front(this);
}

bool
Jdb_module::match(char const *cmd, char const *match, bool full)
{
  if (!cmd || !*cmd || !match)
    return false;

  while (*cmd && *match && *match != ' ')
    {
      if (*cmd != *match)
	return false;

      ++cmd;
      ++match;
    }

  if ((!*match || *match == ' ') && !*cmd)
    return true;

  if (!full && !*match)
    return true;

  return false;
}

Jdb_module::Cmd const*
Jdb_module::has_cmd(char const* cmd, bool short_mode, bool full) const
{
  int n = num_cmds();
  Cmd const* cs = cmds();
  for (int i = 0; i < n; ++i)
    {
      if (short_mode)
	{
	  if (match(cs[i].scmd, cmd))
	    return cs + i;
	}
      else
	{
	  if (match(cs[i].cmd, cmd, full))
	    return cs + i;
	}
    }

  return nullptr;
}

