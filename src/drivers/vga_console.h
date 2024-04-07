#pragma once

#include "types.h"
#include "console.h"

/**
 * Console implementation for VGA.
 *
 * This console is a output only console for VGA.
 * It implements an ANSI ESC capable output device.
 */
class Vga_console : public Console
{
public:

  /**
   * Clear the screen,
   */
  void clear();

  /**
   * Scroll the screen n lines up.
   * @param n number of lines to scroll.
   */
  void scroll(unsigned n);

  /**
   * Get the base address for the VGA memory.
   * @return The base address of the VGA memory.
   */
  Address video_base() const;

  /**
   * Set the base address of the VGA memory.
   * @param base the base address of the VGA memory.
   */
  void video_base(Address base);

  /**
   * Create a new instance of a VGA console.
   * @param base the base address of the VGA memory.
   * @param width the width of the screen.
   * @param height the height of the screen.
   * @param light_white if set to true the color
   *        grey is replaced with white.
   * @param use_color says whether ANSI ESC colors should be
   *        passed (=true) to the video memory or not (=false).
   */
  Vga_console(Address base, unsigned width = 80, unsigned height = 25,
              bool light_white = false, bool use_color = false);

  /**
   * dtor.
   */
  ~Vga_console();

  /**
   * Output method.
   */
  int write(char const *str, size_t len) override;

  /**
   * Empty implementation.
   */
  int getchar(bool blocking = true) override;

  bool is_working() const
  {
    return _is_working;
  }

  /**
   * Output a character.
   */
  void printchar(unsigned x, unsigned y, unsigned char c, unsigned char a)
  {
    set(x + y * _width, c, a);
  }

  Mword get_attributes() const override
  {
    return DIRECT | OUT;
  }

private:

  /// Type of a on screen character.
  struct VChar {
    char c;
    char a;
  } __attribute__((packed));

  VChar   *_video_base;
  Address  _crtc;
  unsigned _width, _height;
  unsigned _x, _y;
  unsigned _attribute;
  enum
  {
    MAX_ANSI_ESC_ARGS = 5,
  };

  int ansi_esc_args[MAX_ANSI_ESC_ARGS];
  unsigned num_ansi_esc_args;

  void (Vga_console::*wr)(char const *, size_t, unsigned &);

  bool const _light_white;
  bool const _use_color;
  bool _is_working;

  /**
   * Set blinking screen cursor
   */
  void blink_cursor(unsigned x, unsigned y);
  void esc_write(char const *str, size_t len, unsigned &i);
  void ansi_esc_write(char const *str, size_t len, unsigned &i);
  void normal_write(char const *str, size_t len, unsigned &i);

  void set(unsigned i, char c, char a)
  {
    _video_base[i].c = c;
    _video_base[i].a = a;
  }

  int seq_6(char const *str, size_t len, unsigned &pos)
  {
    if (pos + 2 >= len)
      return 0;
    _y = str[pos + 1];
    _x = str[pos + 2];
    if (_y >= _height)
      _y = _height - 1;
    if (_x >= _width)
      _x = _width - 1;
    pos += 2;
    return 1;
  }

  int seq_1(char const *, size_t, unsigned &)
  {
    _x = 0;
    _y = 0;
    return 1;
  }

  int seq_5(char const *, size_t, unsigned &)
  {
    for (unsigned i = 0; i < _width - _x; ++i)
      set(_x + (_y * _width) + i, 0x20, _attribute);

    return 1;
  }

  void ansi_attrib(int a)
  {
    char const colors[] = { 0, 4, 2, 6, 1, 5, 3, 7 };

    if (!_use_color && a >= 30 && a <= 47)
      return;

    switch (a)
    {
    case 0:
      if (_light_white)
        _attribute = 0x0f;
      else
        _attribute = 0x07;
      break;
    case 1:
      _attribute |= 0x0808;
      break;
    case 22:
      _attribute &= ~0x0808;
      break;
    case 5:
      _attribute |= 0x8080;
      // FALLTHRU
    default:
      if (30 <= a && a <= 37)
        _attribute = (_attribute & 0x0f0) | colors[a - 30] | ((_attribute >> 8) & 0x08);
      else if (40 <= a && a <= 47)
        _attribute = (_attribute & 0x0f) | (colors[a - 40] << 4) | ((_attribute >> 8) & 0x80);
      break;
    };
  }
};


