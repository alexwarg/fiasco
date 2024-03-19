#pragma once

#include "console.h"
#include "types.h"
#include "delayloop.h"

class Filter_console : public Console
{
public:
  explicit Filter_console(Console *o, int to = 10)
  : Console(ENABLED), _o(o), csi_timeout(to), state(NORMAL), pos(0), arg(0)
  {
    if (o->failed())
      fail();
  }

  ~Filter_console() = default;

  int char_avail() const override
  {
    if (!(_o->state() & INENABLED))
      return -1;

    if (pos)
      return 1;

    return _o->char_avail();
  }

  int write(char const *str, size_t len) override
  {
    if (!(_o->state() & OUTENABLED))
      return len;

    return _o->write(str, len);
  }

  int getchar(bool b = true) override;

  Mword get_attributes() const override
  {
    return _o->get_attributes();
  }

private:
  Console *const _o;
  int csi_timeout;
  enum State
  {
    NORMAL,
    UNKNOWN_ESC,
    GOT_CSI, ///< control sequence introducer
  };

  State state;
  unsigned pos;
  char ibuf[32];
  unsigned arg;
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
};

