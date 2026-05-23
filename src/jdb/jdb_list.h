#pragma once

#include "string_buffer.h"
#include "jdb_regex.h"
#include <jdb_screen.h>

class Jdb_list
{
public:
  Jdb_list() : _start(0), _current(0), _screen_height(Jdb_screen::height() - 4)
  {
    _filter_str[0] = 0;
  }

  // set _t_start element of list
  void set_start(void *start)
  {
    _start = start;
  }

  // _t_start-- if possible
  bool line_back()
  { return filtered_seek(-1, &_start); }

  // _t_start++ if possible
  bool line_forw()
  {
    if (filtered_seek(1, &_last))
      return filtered_seek(1, &_start);
    return false;
  }

  // _t_start -= 24 if possible
  bool page_back()
  { return filtered_seek(-_screen_height, &_start); }

  // _t_start += 24 if possible
  bool page_forw()
  {
    int fwd = filtered_seek(_screen_height, &_last);
    if (fwd)
      return filtered_seek(fwd, &_start);
    return false;
  }

  // _t_start = first element of list
  bool goto_home()
  { return filtered_seek(-99999, &_start); }

  // _t_start = last element of list
  bool goto_end()
  {
    int fwd = filtered_seek(99999, &_last);
    if (fwd)
      return filtered_seek(fwd, &_start);
    return false;
  }

  int page_show();
  void show_header();
  void do_list();

  virtual char const *get_mode_str() const { return "[std mode]"; }
  virtual void next_mode() {}
  virtual void next_sort() {}
  virtual void *get_head() const = 0;
  virtual void show_item(String_buffer *buffer, String_buffer *help_text,
                         void *item) const = 0;
  virtual char const *show_head() const = 0;
  virtual int seek(int cnt, void **item) = 0;
  virtual bool enter_item(void * /*item*/) const { return true; }
  virtual void *follow_link(void *a) { return a; }
  virtual bool handle_key(void * /*item*/, int /*keycode*/) { return false; }
  virtual void *parent(void * /*item*/) { return 0; }
  virtual void *get_valid(void *a) { return a; }

private:
  typedef String_buf<256> Line_buf;
  void *_start, *_last;
  void *_current;
  char _filter_str[20];
  unsigned _screen_height;
  Jdb_regex _regex;

  int lookup_in_visible_area(void *search);
  void *index(int y);
  void handle_string_filter_input();
  Line_buf *render_visible(void *i, String_buffer *help_text);
  int print_limit(const char *s, int visible_len);
  void show_line(Jdb_list::Line_buf *b);
  void *get_visible(void *i);
  int filtered_seek(int cnt, void **item, Jdb_list::Line_buf **buf = 0);
};


