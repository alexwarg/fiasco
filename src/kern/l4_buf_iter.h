#pragma once

#include <types.h>
#include <l4_msg_item.h>
#include <l4_fpage.h>
#include <l4_types.h>

class L4_buf_iter
{
public:
  struct Item
  {
    L4_msg_item b;
    Mword d;
    L4_obj_ref target_space;

    Item() : b(0)
#ifndef NDEBUG
	     , d(0)
#endif
    {}
  };

  explicit L4_buf_iter(Utcb const *utcb, unsigned start) noexcept
  : _buf(&utcb->buffers[start]), _max(&utcb->buffers[Utcb::Max_buffers])
  {
    next();
  }

  bool more() const noexcept
  {
    return _buf < _max;
  }

  Item const *get() const noexcept
  {
    return &c;
  }

  unsigned size() const noexcept
  {
    if (c.b.type() == L4_msg_item::Map && c.b.is_small_obj())
      return 1;

    if (!c.b.compound())
      return 2;

    return 3;
  }

  bool next() noexcept
  {
    if (EXPECT_FALSE(_buf >= _max))
      {
        c.b = L4_msg_item(0);
        return false;
      }

    auto const *br = _buf;
    c.b = L4_msg_item(br[0]);
    if (EXPECT_FALSE(c.b.is_void()))
      return false;

    _buf += size();
    if (EXPECT_FALSE(_buf > _max))
      {
        c.b = L4_msg_item(0);
        return false;
      }

    if (c.b.type() == L4_msg_item::Map && c.b.is_small_obj())
      c.d = c.b.get_small_buf().raw();
    else
      {
        c.d = br[1];
        if (c.b.compound())
          c.target_space = L4_obj_ref(br[2]);
      }
    return true;
  }


private:
  Mword const *_buf;
  Mword const *const _max;
  Item c;
};

class L4_snd_item_iter
{
public:
  struct Item
  {
    L4_msg_item b;
    Mword d;

    Item()
    : b(0)
#ifndef NDEBUG
      , d(0)
#endif
    {}
  };

  explicit L4_snd_item_iter(Utcb const *utcb, unsigned offset) noexcept
  : _buf(&utcb->values[offset]),
    _max(&utcb->values[Utcb::Max_words])
  {}

  bool more() const noexcept
  {
    return _buf < _max;
  }

  Item const *get() const noexcept
  {
    return &c;
  }

  bool next() noexcept
  {
    c.b = L4_msg_item(_buf[0]);
    c.d = 0;

    ++_buf;

    if (EXPECT_TRUE(!c.b.is_void()))
      {
        if (EXPECT_FALSE(_buf >= _max))
          return false;

        c.d = _buf[0];
      }

    ++_buf;
    return true;
  }

private:
  Mword const *_buf;
  Mword const *const _max;
  Item c;
};

