#include <outer_cache-l2cxx0.h>
#include <outer_cache-l2cxx0-bsp.h>
#include <kmem.h>
#include <mem_layout.h>
#include <config.h>
#include <static_init.h>
#include <cstdio>

namespace Outer_cache
{
  namespace Priv
  {
    Static_object<L2cxx0> l2cxx0;

    bool need_sync;
    unsigned waymask;

    namespace {
    static void show_info(unsigned ways, Mword cache_id, Mword aux)
    {
#ifdef CONFIG_JDB
      printf("L2: ID=%08lx Type=%08lx Aux=%08lx WMask=%x S=%d\n",
             cache_id, l2cxx0->read<Mword>(L2cxx0::CACHE_TYPE), aux, waymask, need_sync);

      const char *type;
      switch ((cache_id >> 6) & 0xf)
        {
        case 1:
          type = "210";
          break;
        case 2:
          type = "220";
          break;
        case 3:
          type = "310";
          if ((cache_id & 0x3f) == 5)
            printf("L2: r3p0\n");
          break;
        default:
          type = "Unknown";
          break;
        }

      unsigned waysize = 16 << (((aux >> 17) & 7) - 1);
      printf("L2: Type L2C-%s Size = %dkB  Ways=%d Waysize=%d\n",
             type, ways * waysize, ways, waysize);
#endif
    }

    static void initialize(bool v = true)
    {
      need_sync = true;

      Mword aux = platform_init();
      Mword cache_id = l2cxx0->read<Mword>(L2cxx0::CACHE_ID);
      unsigned ways = 8;

      switch ((cache_id >> 6) & 0xf)
        {
        case 1:
          ways = (aux >> 13) & 0xf;
          break;
        case 3:
          need_sync = false;
          ways = aux & (1 << 16) ? 16 : 8;
          break;
        default:
          break;
        }

      waymask = (1 << ways) - 1;

      l2cxx0->write<Mword>(0, L2cxx0::INTERRUPT_MASK);
      l2cxx0->write<Mword>(~0UL, L2cxx0::INTERRUPT_CLEAR);

      if (!(l2cxx0->read<Mword>(L2cxx0::CONTROL) & 1))
        {
          l2cxx0->write(aux, L2cxx0::AUX_CONTROL);
          invalidate();
          l2cxx0->write<Mword>(1, L2cxx0::CONTROL);
        }

      platform_init_post();

      if (v)
        show_info(ways, cache_id, aux);
    }

    STATIC_INITIALIZER_P(initialize, STARTUP_INIT_PRIO);
    }
  }
}



