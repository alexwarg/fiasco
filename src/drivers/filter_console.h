#pragma once

#include "console.h"
#include "types.h"
#include "delayloop.h"

class Filter_console : public Console
{
public:
  explicit Filter_console(Console *o)
  : Console(ENABLED), _o(o)
  {
    if (o->failed())
      fail();
  }

  ~Filter_console() = default;

  int write(char const *str, size_t len) override
  {
    if (!(_o->state() & OUTENABLED))
      return len;

    return _o->write(str, len);
  }

  Mword get_attributes() const override
  {
    return _o->get_attributes();
  }

private:
  Console *const _o;

#ifdef CONFIG_INPUT
public:
  int char_avail() const override
  {
    if (!(_o->state() & INENABLED))
      return -1;

    if (pos)
      return 1;

    return _o->char_avail();
  }

  int getchar(bool b = true) override;

private:
  int csi_timeout = 10;
  enum State
  {
    NORMAL,
    UNKNOWN_ESC,
    GOT_CSI, ///< control sequence introducer
  };

  State state = NORMAL;
  unsigned pos = 0;
  char ibuf[32];
  unsigned arg = 0;
  int args[4];

  int getchar_timeout(unsigned timeout)
  {
    if (!(_o->state() & INENABLED))
      return -1;

    int c;
    while ((c = _o->getchar(false)) == -1 && timeout--)
      Delay::delay(1);
    return c;
  }
#endif
};

