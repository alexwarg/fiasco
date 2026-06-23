#pragma once

#include "jdb_module.h"
#include "jdb_kobject.h"
#include "jdb_table.h"
#include "paging.h"
#include "space.h"
#include "types.h"

class Jdb_ptab : public Jdb_table
{
public:
  Jdb_ptab(void *pt_base = nullptr, Space *task = nullptr,
           unsigned char pt_level = 0, unsigned entries = 0,
           Address virt_base = 0, int level = 0);

  unsigned col_width(unsigned column) const override;
  unsigned long cols() const override;
  unsigned long rows() const override;
  void draw_entry(unsigned long row, unsigned long col) override;
  void print_statline(unsigned long row, unsigned long col) override;
  unsigned key_pressed(int c, unsigned long &row, unsigned long &col) override;

private:
  Address base;
  Address virt_base;
  int _level;
  Space *_task;
  unsigned entries;
  unsigned char cur_pt_level;
  char dump_raw;

  static unsigned entry_is_pt_ptr(Pdir::Pte_ptr const &entry,
                                  unsigned *entries, unsigned *next_level);
  static Address   entry_phys(Pdir::Pte_ptr const &entry);
  static void     *entry_virt(Pdir::Pte_ptr const &entry);

  void print_entry(Pdir::Pte_ptr const &entry);
  void print_head(void *entry);
  void print_invalid();
  Address disp_virt(int idx);

  inline int index(unsigned row, unsigned col)
  {
    Mword e = (col-1) + (row * (cols()-1));
    if (e < Pdir::Levels::length(cur_pt_level))
      return static_cast<int>(e);
    else
      return -1;
  }

  inline void *pte(int index)
  {
    return reinterpret_cast<void *>(base + index * Pdir::Levels::entry_size(cur_pt_level));
  }
};


class Jdb_ptab_m : public Jdb_module, public Jdb_kobject_handler
{
public:
  Jdb_ptab_m() FIASCO_INIT;

  bool handle_key(Kobject_common *o, int code) override;
  char const *help_text(Kobject_common *o) const override;
  Jdb_module::Action_code action(int cmd, void *&args, char const *&fmt,
                                 int &next_char) override;
  Jdb_module::Cmd const *cmds() const override;
  int num_cmds() const override;

private:
  Address task;
  static char first_char;
  bool show_kobject(Kobject_common *, int) override { return false; }
};
