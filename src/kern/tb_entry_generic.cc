
#include "tb_entry.h"

Mword (*Tb_entry::rdcnt1)() = dummy_read_pmc;
Mword (*Tb_entry::rdcnt2)() = dummy_read_pmc;
Tb_entry_formatter const *Tb_entry_formatter::_fixed[Tbuf_dynentries];

void
Tb_entry_formatter::set_fixed(unsigned type, Tb_entry_formatter const *f)
{
  if (type >= Tbuf_dynentries)
    return;

  _fixed[type] = f;
}

void
Tb_entry::set_rdcnt(int num, Mword (*f)())
{
  if (!f)
    f = dummy_read_pmc;

  switch (num)
    {
    case 0: rdcnt1 = f; break;
    case 1: rdcnt2 = f; break;
    }
}

