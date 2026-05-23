
#pragma once

#include <globalconfig.h>

#ifndef CONFIG_JDB_REGEX

class Jdb_regex
{
public:
  static bool avail() { return false; }

  bool start(const char *) { return true; }
  bool find(const char *, const char **, const char **) { return false; }
};

#else

#include "initcalls.h"
#include "regex.h"

class Jdb_regex
{
public:
  static bool avail() { return true; }

  ~Jdb_regex()
  {
    if (_active)
      regfree(&_r);
  }

  bool start(const char *searchstr);
  void finish();
  bool find(const char *buffer, const char **beg, const char **end);

  regex_t     _r;
  regmatch_t  _matches[1];
  bool        _active = false;
};


#endif
