#include "timeout.h"
#include "kdb_ke.h"

DEFINE_PER_CPU Per_cpu<Timeout_q> Timeout_q::timeout_queue;


/* Yeah, i know, an derived and specialized timeout class for
   the root node would be nicer. I already had done this, but
   it was significantly slower than this solution */
bool
Timeout::expired()
{
  kdb_ke("Wakeup List Terminator reached");
  return false;
}


