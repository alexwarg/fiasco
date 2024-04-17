#pragma once

#include <types.h>

class Boot_info
{
public:
  static void set_checksum_ro(unsigned ro_cs)
  {  _checksum_ro = ro_cs; }

  static void set_checksum_rw(unsigned rw_cs)
  {  _checksum_rw = rw_cs; }

  static unsigned get_checksum_ro()
  {
    return _checksum_ro;
  }

  static unsigned get_checksum_rw()
  {
    return _checksum_rw;
  }

  static void reset_checksum_ro();

private:
  static unsigned _checksum_ro;
  static unsigned _checksum_rw;
};

