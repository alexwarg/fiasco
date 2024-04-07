#include "vga_console.h"

#include <cstring>
#include <cctype>

#include "io.h"
#include "mem_layout.h"

Vga_console::Vga_console(Address vbase, unsigned width, unsigned height,
                         bool light_white, bool use_color)
  : Console(ENABLED), _video_base((VChar *)vbase), _width(width), _height(height),
    _x(0), _y(height - 1), _attribute(light_white ? 0x0f : 0x07),
    wr(&Vga_console::normal_write), _light_white(light_white),
    _use_color(use_color), _is_working(false)
{
  unsigned volatile *vm = (unsigned volatile *)_video_base;
  unsigned x = *vm;
  *vm = 12;
  if (*vm != 12)
    {
      _is_working = false;
      *vm = x;
      return;
    }

  *vm = 47634;
  if (*vm != 47634)
    {
      _is_working = false;
      *vm = x;
      return;
    }

  _is_working = true;
  *vm = x;

  if (vbase == Mem_layout::Adap_vram_mda_beg)
    _crtc = 0x3b4;
  else
    _crtc = 0x3d4;

  scroll(1);
}

void Vga_console::scroll(unsigned lines)
{
  if (lines && lines <= _height)
    {
      for (unsigned i = lines * _width; i < _width * _height; ++i)
        _video_base[i-lines*_width] = _video_base[i];
#if 0
    memmove(_video_base, _video_base + _width*lines,
            _width*(_height - lines)*2 );
#endif

      for (unsigned i = (_height - lines) * _width; i < _width * _height; ++i)
        set(i, 0x20, _attribute);
    }
}

void Vga_console::blink_cursor(unsigned x, unsigned y)
{
  unsigned pos = 80 * y + x;

  Io::out16( (pos & 0xff00) | 0x0e, _crtc);
  Io::out16( (pos <<     8) | 0x0f, _crtc);
}

void Vga_console::clear()
{
  for (unsigned i = 0; i < _width * _height; ++i)
    set(i, 0x20, _attribute);
}






void Vga_console::esc_write(char const *str, size_t len, unsigned &i)
{
  if (i >= len)
    return;

  if (str[i] == '[')
    {
      ansi_esc_args[0] = 0;
      ansi_esc_args[1] = 0;
      num_ansi_esc_args = 0;
      wr = &Vga_console::ansi_esc_write;
    }
  else
    wr = &Vga_console::normal_write;

  ++i;
}

void Vga_console::ansi_esc_write(char const *str, size_t len, unsigned &i)
{
  /* ANSI seq */
  for (; i < len && (isdigit(str[i]) || str[i] == ';'); ++i)
    {
      if (str[i] == ';')
        ansi_esc_args[++num_ansi_esc_args] = 0;
      else if (num_ansi_esc_args < MAX_ANSI_ESC_ARGS)
        ansi_esc_args[num_ansi_esc_args]
          = ansi_esc_args[num_ansi_esc_args] * 10 + (str[i] - '0');
    }

  if (i >= len)
    return;

  switch (str[i])
  {
  case 'f': /* Move cursor + blink cursor to location v,h */
    _x = ansi_esc_args[1];
    if (_y)
      --_y;
    _y = ansi_esc_args[0];
    if (_x)
      --_x;
    if (_x >= _width)
      _x = _width - 1;
    if (_y >= _height)
      _y = _height - 1;
    blink_cursor(_x, _y);
    break;

  case 'H': /* Move cursor to screen location v,h */
    _x = ansi_esc_args[1];
    if (_x)
      --_x;
    _y = ansi_esc_args[0];
    if (_y)
      --_y;
    if (_x >= _width)
      _x = _width - 1;
    if (_y >= _height)
      _y = _height - 1;
    break;

  case 'm': /*color*/
    {
      int p = 0;
      do {
        ansi_attrib(ansi_esc_args[p++]);
      } while (num_ansi_esc_args--);
    }
    break;

  case 'X': /* clear n characters */
    for (unsigned i = _x + (_y * _width);
         i < _x + (_y * _width) + ansi_esc_args[0]; ++i)
      set(i, 0x20, _attribute);

    break;

  case 'K': /* Clear line from cursor right */
    switch (ansi_esc_args[0])
    {
    default:
    case 0:
      for (unsigned i = _x + (_y * _width); i < (_y * _width) + _width; ++i)
        set(i, 0x20, _attribute);
      break;

    case 1:
      for (unsigned i = (_y * _width); i < (_y * _width) + _x; ++i)
        set(i, 0x20, _attribute);
      break;

    case 2:
      for (unsigned i = (_y * _width); i < (_y * _width) + _width; ++i)
        set(i, 0x20, _attribute);
      break;

    }
    break;

  case 'J': /* Clear screen from cursor */
    switch (ansi_esc_args[0])
    {
    default:
    case 0:
      for (unsigned i = (_y * _width);
           i < (_y * _width) + _width * (_height - _y); ++i)
        set(i, 0x20, _attribute);
      break;

    case 1:
      for (unsigned i = 0; i < _width * _y; ++i)
        set(i, 0x20, _attribute);
      break;

    case 2:
      for (unsigned i = 0; i < _width * _height; ++i)
        set(i, 0x20, _attribute);
      break;
    }
    break;
  }

  wr = &Vga_console::normal_write;
  ++i;
}

void Vga_console::normal_write(char const *str, size_t len, unsigned &i)
{
  for (; i < len; ++i)
    {
      switch (str[i])
      {
#if 0
        case '\b':
          if(_x) 
            --_x;
          else if(_y) {
            --_y;_x=0;
          }
          break;
#endif	
        case '\n':
        case '\f':
          _x = 0;
          ++_y;
          if (_y >= _height)
            {
              scroll(_y - _height + 1);
              _y = _height - 1;
            }
          break;
        case '\r':
          _x = 0;
          break;
        case '\t':
          _x = (_x & ~7) + 8; 
          break;

        case 27: /* ESC */
          wr = &Vga_console::esc_write;
          ++i;
          return;
          break;

        case 6: /* cursor */
          seq_6(str, len, i);
          break;

        case 1: /* home */
          seq_1(str, len, i);
          break;

        case 5: /* clear to end of line */
          seq_5(str, len, i);
          break;

        case 8: /* back space */
          if (_x)
            --_x;
          break;

        default:
          if ((unsigned)str[i] >= 32)
            {
              if (_x >= _width)
                {
                  ++_y;
                  _x = 0;
                }

              if (_y >= _height)
                {
                  scroll(_y - _height + 1);
                  _y = _height - 1;
                }

              if (_x < _width)
                set(_y * _width + _x,
                    str[i] == '\265' ? '\346' : str[i], _attribute);

              ++_x;
            }
          break;
      }
    }
}


int Vga_console::write(char const *str, size_t len)
{
  unsigned pos = 0;
  while (pos < len)
    (this->*wr)(str, len, pos);

  return pos;
}

int Vga_console::getchar(bool)
{
  blink_cursor(_x, _y);
  return -1;
}

Vga_console::~Vga_console()
{}

