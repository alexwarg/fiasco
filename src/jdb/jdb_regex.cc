#include <jdb_regex.h>

#include <cstring>

#include "config.h"
#include "jdb_module.h"
#include "kmem_alloc.h"
#include "panic.h"
#include "simple_malloc.h"

bool
Jdb_regex::start(const char *searchstr)
{
  if (!searchstr || !*searchstr)
    return true;

  finish();

  // compile expression
  if (regcomp(&_r, searchstr, REG_EXTENDED) == 0)
    return (_active = true);

  return false;
}

void
Jdb_regex::finish()
{
  if (_active)
    regfree(&_r);
  memset(_matches, 0, sizeof(_matches));
  _active = false;
}

bool
Jdb_regex::find(const char *buffer, const char **beg, const char **end)
{
  if (!_active)
    return false;

  // execute expression
  int ret = regexec(&_r, buffer, cxx::size(_matches), _matches, 0);

  if (ret == REG_NOMATCH)
    return false;

  if (beg)
    *beg = buffer + _matches[0].rm_so;
  if (end)
    *end = buffer + _matches[0].rm_eo;
  return true;
}
