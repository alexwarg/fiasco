
#include "mux_console.h"
#include "processor.h"

#include <cstdio>

#ifdef CONFIG_INPUT
int
Mux_console::getchar(bool blocking)
{
  if (_next_getchar != -1)
    {
      int c = _next_getchar;
      _next_getchar = -1;
      return c;
    }

  int ret = -1;
  do
    {
      int conscnt = 0;
      for (int i = 0; i < _items; ++i)
        if (_cons[i] && (_cons[i]->state() & INENABLED))
          {
            ret = _cons[i]->getchar(false);
            if (ret != -1)
              return ret;
            ++conscnt;
          }

      if (!conscnt)
        break;

      if (blocking)
        Proc::pause();
    }
  while (blocking && ret == -1);

  return ret;
}

int
Mux_console::char_avail() const
{
  int ret = -1;
  for (int i = 0; i < _items; ++i)
    if (_cons[i] && (_cons[i]->state() & INENABLED))
      {
        int tmp = _cons[i]->char_avail();
        if (tmp == 1)
          return 1;
        else if (tmp == 0)
          ret = tmp;
      }
  return ret;
}
#endif

void
Mux_console::list_consoles()
{
  for (int i = 0; i < _items; i++)
    if (_cons[i])
      {
        Mword attr = _cons[i]->get_attributes();

        printf("  " L4_PTR_FMT "  %s  (%s)  ",
               attr, _cons[i]->str_mode(), _cons[i]->str_state());
        for (unsigned bit = 2; bit < sizeof(attr) * 4; bit++)
          if (attr & (1 << bit))
            printf("%s ", Console::str_attr(bit));
        putchar('\n');
      }
}

