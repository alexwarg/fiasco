
#include <jdb_kern_info.h>
#include <cxx/slist>

#include <cctype>
#include <cstdio>

#include <cpu.h>
#include <jdb.h>
#include <jdb_module.h>
#include <static_init.h>
#include <kmem_alloc.h>


//===================
// Std JDB modules
//===================


Jdb_kern_info_module::Jdb_kern_info_module(char subcmd, char const *descr)
{
  _subcmd = subcmd;
  _descr  = descr;
}


char Jdb_kern_info::_subcmd;
Jdb_kern_info::Module_list Jdb_kern_info::modules;

void
Jdb_kern_info::register_subcmd(Jdb_kern_info_module *m)
{
  Module_iter p;
  for (p = modules.begin();
       p != modules.end()
       && (tolower(p->_subcmd) < tolower(m->_subcmd)
           || (tolower(p->_subcmd) == tolower(m->_subcmd)
               && p->_subcmd > m->_subcmd));
      ++p)
    ;

  modules.insert_before(m, p);
}

Jdb_module::Action_code
Jdb_kern_info::action(int cmd, void *&args, char const *&, int &)
{
  if (cmd != 0)
    return NOTHING;

  char c = *static_cast<char*>(args);

  for (auto const &&kim: modules)
    {
      if (kim->_subcmd == c)
	{
	  putchar('\n');
	  kim->show();
	  putchar('\n');
	  return NOTHING;
	}
    }

  putchar('\n');
  for (auto const &&kim: modules)
    printf("  k%c   %s\n", kim->_subcmd, kim->_descr);

  putchar('\n');
  return NOTHING;
}

int
Jdb_kern_info::num_cmds() const
{
  return 1;
}

Jdb_module::Cmd const *
Jdb_kern_info::cmds() const
{
  static Cmd cs[] =
    {
	{ 0, "k", "k", "%c",
	   "k\tshow various kernel information (kh=help)", &_subcmd }
    };

  return cs;
}

Jdb_kern_info::Jdb_kern_info()
  : Jdb_module("INFO")
{}

static Jdb_kern_info jdb_kern_info INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
