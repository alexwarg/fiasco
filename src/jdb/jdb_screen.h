#pragma once

#include "types.h"

class Jdb_screen
{
public:
  static const unsigned Mword_size_bmode = sizeof(Mword) * 2;
  static const unsigned Mword_size_cmode = sizeof(Mword);
  static const unsigned Col_head_size = sizeof(Mword) * 2;

  static const char *Mword_adapter;
  static const char *Mword_not_mapped;
  static const char *Mword_blank;

  static const char *const Reg_names[];
  static const char Reg_prefix;
  static const char *Line;

  static const char *Root_page_table;

  static int num_regs();

  static void set_height(unsigned int h) { _height = h; }
  static void set_width(unsigned int w)  { _width = w; }

  static inline unsigned int width()  { return _width; }
  static inline unsigned int height() { return _height; }

  static inline unsigned long cols(unsigned headsize, unsigned entrysize)
  { return (width() - headsize) / (entrysize + 1) + 1; }

  static inline unsigned long cols()
  { return cols(Col_head_size, Mword_size_bmode); }

  static inline unsigned long rows()
  { return ~0UL / ((cols() - 1) * 4) + 1; }

  static inline void enable_direct(bool enable) { _direct_enabled = enable; }
  static inline bool direct_enabled()           { return _direct_enabled; }

private:
  static unsigned int _height;
  static unsigned int _width;
  static bool         _direct_enabled;
};
