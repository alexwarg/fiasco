
#include <jdb_ansi.h>

// preserve the history of the serial console if fancy != 0
void
Jdb_ansi::clear_screen(int fancy)
{
  if (fancy == FANCY)
    {
      cursor(Jdb_screen::height(), 1);
      for (unsigned i=0; i<Jdb_screen::height(); i++)
	{
	  putchar('\n');
	  clear_to_eol();
	}
    }
  else
    {
      cursor();
      for (unsigned i=0; i<Jdb_screen::height()-1; i++)
	{
	  clear_to_eol();
	  putchar('\n');
	}
    }
  cursor();
}

void
Jdb_ansi::line()
{
  for (unsigned i = 0; i < Jdb_screen::width(); ++i)
    putchar('-');
}
