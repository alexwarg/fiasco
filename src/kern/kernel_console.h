#pragma once

#include "mux_console.h"
#include "std_macros.h"

class Kconsole : public Mux_console
{
public:
  Kconsole()
  {
    Console::stdout = this;
    Console::stderr = this;
    Console::stdin  = this;
  }
  static void init();

  int  getchar(bool blocking = true) override;

  static Kconsole *console() FIASCO_CONST
  { return _c; }

  void set_ignore_input(Unsigned64 until = ~0ULL)
  {
    _ignore_input_until = until;
  }

private:
  Unsigned64 _ignore_input_until = 0;

  static Static_object<Kconsole> _c;

  int check_input_ignore();
};

