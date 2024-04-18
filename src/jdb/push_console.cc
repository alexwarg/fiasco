
#include <push_console.h>
#include <keycodes.h>

int
Push_console::getchar(bool /*blocking*/)
{
  if (_out != _in)
    {
      int c = _buffer[_out++];
      if (_out >= (int)sizeof(_buffer))
        _out = 0;

      return c == '_' ? KEY_RETURN : c;
    }

  return -1; // no keystroke available
}

int
Push_console::char_avail() const
{
  return _in != _out; // unknown
}

int
Push_console::write(char const * /*str*/, size_t len)
{
  return len;
}

void
Push_console::push(char c)
{
  int _ni = _in + 1;
  if (_ni >= (int)sizeof(_buffer))
    _ni = 0;

  if (_ni == _out) // buffer full
    return;

  _buffer[_in] = c;
  _in = _ni;
}

