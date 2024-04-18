#pragma once

#include "console.h"

class Push_console : public Console
{
public:
  Push_console() : Console(ENABLED) {}

  int getchar(bool blocking) override;
  int char_avail() const override;
  int write(char const *str, size_t len) override;
  Mword get_attributes() const override
  {
    return PUSH | IN;
  }

  void push(char c);

  void flush()
  {
    _in = _out = 0;
  }

private:
  char _buffer[256];
  int _in;
  int _out;
};

