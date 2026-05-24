#pragma once

class Jdb_table
{
public:
  enum
  {
    Nothing = 0,
    Handled,
    Redraw,
    Edit,
    Back,
    Exit,
  };

  explicit Jdb_table(int show_obj_help = 0)
   : _show_obj_help(show_obj_help)
  {}

  bool show(unsigned long crow, unsigned long ccol);
  void draw_table(unsigned long row, unsigned long col,
                  unsigned lines, unsigned columns);

  virtual unsigned col_width(unsigned col) const = 0;
  virtual unsigned long cols() const = 0;
  virtual unsigned long rows() const = 0;
  virtual char col_sep(unsigned col) const;
  virtual void draw_entry(unsigned long row, unsigned long col) = 0;
  virtual unsigned key_pressed(int key, unsigned long &row, unsigned long &col);
  virtual void print_statline(unsigned long row, unsigned long col) = 0;
  virtual bool has_row_labels() const;
  virtual bool has_col_labels() const;
  virtual unsigned width() const;
  virtual unsigned height() const;

  virtual bool edit_entry(unsigned long row, unsigned long col, unsigned cx, unsigned cy);

private:
  unsigned long vis_cols(unsigned long first_col, unsigned long *w);
  unsigned col_ofs(unsigned long first_col, unsigned long col);

  int _show_obj_help;
};
