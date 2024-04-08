#pragma once

#include "console.h"

/**
 * Platform independent keyboard stub.
 *
 * Provides an empty implentation for write(...).
 */
class Keyb : public Console
{
public:
  // must be implemented in platform part.
  int getchar(bool blocking = true) override;

  // implemented empty
  int write(char const *str, size_t len) override
  {
    (void) str;
    return len;
  }

  Mword get_attributes() const override
  {
    return DIRECT | IN;
  }

  enum Keymap { Keymap_en, Keymap_de };
  void set_keymap(Keymap);

  Keyb() : Console(ENABLED) {}
};

