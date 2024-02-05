#pragma once

#include <cstdio>
#include <simpleio.h>
#include "jdb_screen.h"

class Jdb_ansi
{
public:
  enum
  {
    NOFANCY=0,
    FANCY=1
  };

  enum Direction
  {
    Cursor_up = 'A',
    Cursor_down = 'B',
    Cursor_right = 'C',
    Cursor_left = 'D'
  };

  static void cursor(Direction d, unsigned n = 1)
  {
    printf("\033[%u%c", n, static_cast<char>(d));
  }

  static void cursor(unsigned int row=0, unsigned int col=0)
  {
    if (row || col)
      printf ("\033[%u;%uH", row, col);
    else
      printf ("\033[%u;%uH", 1u, 1u);
  }

  static void blink_cursor(unsigned int row, unsigned int col)
  {
    printf ("\033[%u;%uf", row, col);
  }

  static void cursor_save()
  {
    putstr ("\0337");
  }

  static void cursor_restore()
  {
    putstr ("\0338");
  }

  static void screen_erase()
  {
    putstr ("\033[2J");
  }

  static void screen_scroll (unsigned int start, unsigned int end)
  {
    if (start || end)
      printf ("\033[%u;%ur", start, end);
    else
      printf ("\033[r");
  }

  static void clear_to_eol()
  {
    putstr(clear_to_eol_str());
  }

  static char const *clear_to_eol_str()
  {
    return "\033[K";
  }

  static char const *clear_to_eol_lf_str()
  {
    return "\033[K\n";
  }

  // preserve the history of the serial console if fancy != 0
  static void clear_screen(int fancy=FANCY);
  static void line();
};
