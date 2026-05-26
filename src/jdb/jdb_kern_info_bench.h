#pragma once

#include <jdb.h>
#include <jdb_kern_info.h>
#include <cstdio>

class Jdb_kern_info_bench : public Jdb_kern_info_module
{
public:
  Jdb_kern_info_bench();
  void show() override;

private:
  void do_mp_benchmark();

  static void show_arch();
};

