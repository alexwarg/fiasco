#pragma once

#include <cstdio>
#include <cstdarg>

class String_buffer
{
public:
  String_buffer() = default;
  constexpr String_buffer(char *buf, int len) : _buf(buf), _len(len) {}

  constexpr int space() const { return _len; }
  constexpr char *remaining_buffer() const { return _buf; }
  constexpr void reset(char *buf, int len) { _buf = buf; _len = len; }
  constexpr bool append(char c)
  {
    if (_len <= 0)
      return false;

    --_len;
    *(_buf++) = c;
    return true;
  }

  constexpr void fill(char c)
  {
    for (; _len > 0; ++_buf, --_len)
      *_buf = c;
  }

  constexpr void terminate()
  {
    if (_len)
      *_buf = 0;
    else
      _buf[-1] = 0;
  }


  bool __attribute__((format(printf, 2, 3))) printf(char const *fmt, ...)
  {
    if (_len <= 0)
      return false;

    va_list list;
    int l;
    va_start(list, fmt);
    l = vsnprintf(_buf, _len, fmt, list);
    va_end(list);
    if (l >= _len)
      {
        _buf += _len;
        _len = 0;
        return false;
      }

    _buf += l;
    _len -= l;
    return true;
  }

private:
  char *_buf = nullptr;
  int _len = 0;
};

template<unsigned LEN>
class String_buf : public String_buffer
{
public:
  constexpr String_buf() : String_buffer(_s, LEN) {}

  constexpr int length() const { return LEN - space(); }
  constexpr char *begin() { return _s; }
  constexpr char const *begin() const { return _s; }
  constexpr char const *end() const { return remaining_buffer(); }
  constexpr void reset() { String_buffer::reset(_s, LEN); }
  constexpr void clear() { reset(); terminate(); }
  constexpr char const *c_str()
  {
    terminate();
    return begin();
  }

private:
  char _s[LEN];
};

