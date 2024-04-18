
#pragma once

#include <kip.h>
#include <types.h>
#include <cstring>
#include <mem_unit.h>

static void
kip_clock_deploy_code_blob(Kip *kip, void const *blob)
{
  for (unsigned char const *p = reinterpret_cast<unsigned char const *>(blob);;)
    {
      if (!p[0]) // nothing to be done
        break;

      unsigned sz = p[0];
      unsigned offset = p[2];

      memcpy(kip->clock_blob() + offset, p + 4, sz);
      Mem_unit::make_coherent_to_pou(kip->clock_blob() + offset, sz);

      if (!p[1]) // no next blob
        break;

      p += p[1];
    }
}

