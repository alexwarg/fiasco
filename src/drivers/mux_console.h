#pragma once

#include "types.h"
#include "console.h"

/**
 * Console multiplexer.
 *
 * This implementation of the Console interface can be used to
 * multiplex among some input, output, and in-out consoles.
 */
class Mux_console : public Console
{
public:

  enum
  {
    SIZE = 8  ///< The maximum number of consoles to be multiplexed.
  };

  struct Save_state
  {
    Mword cons[SIZE];
  };

  Mux_console()
  : Console(ENABLED), _next_getchar(-1), _items(0)
  {}


  int write(char const *str, size_t len) override
  {
    for (int i = 0; i < _items; ++i)
      if (_cons[i] && (_cons[i]->state() & OUTENABLED))
        _cons[i]->write(str, len);

    return len;
  }

  int getchar(bool blocking = true) override;
  int char_avail() const override;

  /**
   * deliver attributes of all subconsoles.
   */
  Mword get_attributes() const override
  {
    Mword attr = 0;

    for (int i = 0; i < _items; i++)
      if (_cons[i])
        attr |= _cons[i]->get_attributes();

    return attr;
  }

  void getchar_chance()
  {
    for (int i = 0; i < _items; ++i)
      if (   _cons[i] && (_cons[i]->state() & INENABLED)
          && _cons[i]->char_avail() == 1)
        {
          int c = _cons[i]->getchar(false);
          if (c != -1 && _next_getchar == -1)
            _next_getchar = c;
        }
  }

  /**
   * Register a console to be multiplexed.
   * @param cons the Console to add.
   * @param pos the position of the console, normally not needed.
   */
  virtual bool register_console(Console *c, int pos = 0)
  {
    if (c->failed())
      return false;

    if (_items >= SIZE)
      return false;

    if (pos >= SIZE || pos < 0)
      return false;

    if (pos > _items)
      pos = _items;

    if (pos < _items)
      for (int i = _items - 1; i >= pos; --i)
        _cons[i + 1] = _cons[i];

    _items++;
    _cons[pos] = c;

    return true;
  }

  /**
   * Unregister a console from the multiplexer.
   * @param cons the console to remove.
   */
  bool unregister_console(Console *c)
  {
    int pos;
    for (pos = 0; pos < _items && _cons[pos] != c; pos++)
      ;
    if (pos == _items)
      return false;

    --_items;
    for (int i = pos; i < _items; ++i)
      _cons[i] = _cons[i + 1];

    return true;
  }

  /**
   * Change the state of a group of consoles specified by
   *        attributes.
   * @param any_true   match if console has any of these attributes
   * @param all_false  match if console doesn't have any of these attributes
   */
  void change_state(Mword any_true, Mword all_false,
                    Mword mask, Mword bits)
  {
    for (int i=0; i<_items; i++)
      {
        if (_cons[i])
          {
            Mword attr = _cons[i]->get_attributes();
            if (   // any bit of the any_true attributes must be set
                   (!any_true  || (attr & any_true)  != 0)
                   // all bits of the all_false attributes must be cleared
                && (!all_false || (attr & all_false) == 0))
              {
                _cons[i]->state((_cons[i]->state() & mask) | bits);
              }
          }
      }
  }

  /**
   * Find a console with a specific attribute.
   * @param any_true match to console which has set any bit of this bitmask
   */
  Console *find_console(Mword any_true)
  {
    for (int i = 0; i < _items; i++)
      if (_cons[i] && _cons[i]->get_attributes() & any_true)
        return _cons[i];

    return 0;
  }

  /**
   * Start exclusive mode for a specific console. Only the one
   *        console which matches to any_true is enabled for input and
   *        output. All other consoles are disabled.
   * @param any_true match to console which has set any bit of this bitmask
   */
  void start_exclusive(Mword any_true)
  {
    // enable exclusive console
    change_state(any_true, 0, ~0UL, (OUTENABLED|INENABLED));
    // disable all other consoles
    change_state(0, any_true, ~(OUTENABLED|INENABLED), 0);
  }

  /**
   * End exclusive mode for a specific console.
   * @param any_true match to console which has set any bit of this bitmask
   */
  void end_exclusive(Mword any_true)
  {
    // disable exclusive console
    change_state(any_true, 0, ~(OUTENABLED|INENABLED), 0);
    // enable all other consoles
    change_state(0, any_true, ~0UL, (OUTENABLED|INENABLED));
  }

  /**
   * Save the state of all muxed consoles.
   */
  void save_state(Save_state *state) const
  {
    for (int i = 0; i < _items; i++)
      if (_cons[i])
        state->cons[i] = _cons[i]->state();
  }

  /**
   * Restore the state of the muxed consoles.
   */
  void restore_state(Save_state const *state)
  {
    for (int i = 0; i < _items; i++)
      if (_cons[i])
        _cons[i]->state(state->cons[i]);
  }

  void list_consoles();

private:
  int     _next_getchar;
  int     _items;
  Console *_cons[SIZE];

  int check_input_ignore();
};

