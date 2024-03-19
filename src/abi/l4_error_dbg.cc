
#include "l4_error.h"

static char const *__errors[] =
{ "OK", "timeout", "not existent", "canceled", "overflow",
  "xfer snd", "xfer rcv", "aborted", "map failed" };

char const *
L4_error::str_error() const
{
  return __errors[(_raw >> 1) & 0xf];
}
