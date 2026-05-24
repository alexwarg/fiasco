#pragma once

#include "initcalls.h"
#include "l4_types.h"
#include "thread.h"

class Tb_entry;
class String_buffer;

class Jdb_tbuf_output
{
public:
  typedef void (Format_entry_fn)(String_buffer *, Tb_entry *tb, const char *tidstr,
                                 int tidlen);
  static void init();
  static void register_ff(Unsigned8 type, Format_entry_fn format_entry_fn);
  // return thread+ip of entry <e_nr>
  static int thread_ip(int e_nr, Thread const **th, Mword *ip);

  static void toggle_names()
  {
    show_names = !show_names;
  }

  static void print_entry(String_buffer *buf, int e_nr);
  static void print_entry(String_buffer *buf, Tb_entry *tb);
  static bool set_filter(const char *filter_str, Mword *entries);

private:
  static Format_entry_fn *_format_entry_fn[];
  static bool show_names;

  static void dummy_format_entry(String_buffer *buf, Tb_entry *tb, const char *, int);
};

