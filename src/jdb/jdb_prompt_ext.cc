#include <jdb_prompt_ext.h>

Jdb_prompt_ext::List Jdb_prompt_ext::exts;

Jdb_prompt_ext::Jdb_prompt_ext()
{
  exts.push_front(this);
}

void Jdb_prompt_ext::do_all()
{
  for (auto const &&e: exts)
    e->ext();
}
