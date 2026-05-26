#pragma once

#include <jdb_types.h>
#include <globalconfig.h>

class Jdb_mips_base
{
public:
  static int is_adapter_memory(Jdb_address) { return 0; }
  static bool handle_special_cmds(int) { return 1; }
  static bool test_checksums() { return true; }


};

using Jdb_arch = Jdb_mips_base;

