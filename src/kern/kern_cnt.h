#pragma once

#include "types.h"

namespace Kern_cnt
{
  enum {
    Valid_ctrs = 7,
  };

  enum {
    Max_slot = 2,
  };

  extern Unsigned32 *kcnt[Max_slot];
  extern Mword (*read_kcnt_fn[Max_slot])();
  extern Unsigned8 valid_ctrs[Valid_ctrs];

  int valid_2_ctr(unsigned num);
  int ctr_2_valid(unsigned num);

  Unsigned32 *get_vld_ctr(int num);
  const char *get_vld_str(unsigned num);

  int mode(Mword slot, const char **mode, const char **name, Mword *event);
  int setup_pmc(Mword slot, Mword event);

  const char *get_str(unsigned num);
  Unsigned32 *get_ctr(int num);
};


