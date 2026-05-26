#pragma once

class Jdb_bp_ia32_bits
{
public:
  enum
  {
    Val_enter      = 0x000000000000ff00,
    Val_leave      = 0x0000000000000000,
    Val_test_sstep = 0x0000000000004000,
    Val_test       = 0x000000000000000f,
    Val_test_other = 0x000000000000e00f,
  };
};
