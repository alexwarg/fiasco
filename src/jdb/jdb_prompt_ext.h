#pragma once

#include <cxx/hlist>

class Jdb_prompt_ext : public cxx::H_list_item
{
public:
  Jdb_prompt_ext();
  virtual void ext() = 0;
  virtual ~Jdb_prompt_ext() = 0;

  static void do_all();

private:
  typedef cxx::H_list_bss<Jdb_prompt_ext> List;
  static List exts;
};

inline Jdb_prompt_ext::~Jdb_prompt_ext() {}
