#pragma once

#include <jdb_module.h>
#include <cxx/slist>

class Jdb_kern_info_module : public cxx::S_list_item
{
  friend class Jdb_kern_info;
public:
  Jdb_kern_info_module(char subcmd, char const *descr) FIASCO_INIT;
private:
  virtual void show(void) = 0;
  char                 _subcmd;
  char const           *_descr;
};

/**
 * 'kern info' module.
 *
 * This module handles the 'k' command, which
 * prints out various kernel information.
 */
class Jdb_kern_info : public Jdb_module
{
public:
  Jdb_kern_info() FIASCO_INIT;

  static void register_subcmd(Jdb_kern_info_module *m);
  Action_code action(int cmd, void *&args, char const *&, int &) override;
  int num_cmds() const override;
  Cmd const *cmds() const;

private:
  typedef cxx::S_list_bss<Jdb_kern_info_module> Module_list;
  typedef Module_list::Iterator Module_iter;
  static char                 _subcmd;
  static Module_list modules;
};



