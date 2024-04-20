#pragma once

#include <paging-page.h>
#include <l4_fpage.h>

/**
 * Mixin for PTE pointers for ARMv5 page-table attributes.
 */
template<typename CLASS, typename Entry>
class Pte_v5_attribs
{
private:
  CLASS const *_this() const { return static_cast<CLASS const *>(this); }
  CLASS *_this() { return static_cast<CLASS *>(this); }

public:
  Entry _attribs_mask() const
  {
    if (_this()->level == 0)
      return ~Entry(0x00000c0c);
    else
      return ~Entry(0x00000ffc);
  }

  Entry _attribs(Page::Attr attr) const
  {
    static const unsigned short perms[] = {
        0x1 << 10, // 0000: none
        0x1 << 10, // 000X: kernel RW (there is no RO)
        0x1 << 10, // 00W0:
        0x1 << 10, // 00WX:

        0x1 << 10, // 0R00:
        0x1 << 10, // 0R0X:
        0x1 << 10, // 0RW0:
        0x1 << 10, // 0RWX:

        0x1 << 10, // U000:
        0x0 << 10, // U00X:
        0x3 << 10, // U0W0:
        0x3 << 10, // U0WX:

        0x0 << 10, // UR00:
        0x0 << 10, // UR0X:
        0x3 << 10, // URW0:
        0x3 << 10  // URWX:
    };

    typedef Page::Type T;
    Mword r = 0;
    if (attr.type == T::Normal())   r |= Page::CACHEABLE;
    if (attr.type == T::Buffered()) r |= Page::BUFFERED;
    if (attr.type == T::Uncached()) r |= Page::NONCACHEABLE;
    if (_this()->level == 0)
      return r | perms[cxx::int_value<L4_fpage::Rights>(attr.rights)];
    else
      {
        Mword p = perms[cxx::int_value<L4_fpage::Rights>(attr.rights)];
        p |= p >> 2;
        p |= p >> 4;
        return r | p;
      }
  }

  Entry _page_bits() const
  { return 2; }

  Page::Attr attribs() const
  {
    auto r = access_once(_this()->pte);
    auto c = r & 0xc;
    r &= 0xc00;

    typedef L4_fpage::Rights R;
    typedef Page::Type T;

    R rights;
    switch (r)
      {
      case 0x000: rights = R::URX(); break;
      default:
      case 0x400: rights = R::RWX(); break;
      case 0xc00: rights = R::URWX(); break;
      }

    T type;
    switch (c)
      {
      default:
      case Page::CACHEABLE: type = T::Normal(); break;
      case Page::BUFFERED:  type = T::Buffered(); break;
      case Page::NONCACHEABLE: type = T::Uncached(); break;
      }
    return Page::Attr(rights, type);
  }

  Page::Rights access_flags() const
  { return Page::Rights(0); }

  void del_rights(L4_fpage::Rights r)
  {
    if (!(r & L4_fpage::Rights::W()))
      return;

    auto p = access_once(_this()->pte);
    if ((p & 0xc00) == 0xc00)
      {
        p &= (_this()->level == 0) ? ~Mword(0xc00) : ~Mword(0xff0);
        write_now(_this()->pte, p);
      }
  }
};

/**
 * Mixin for PTE pointers for ARMv6+ page-table attributes
 * (short descriptors).
 */
template<typename CLASS, typename ATTRIBS>
class Pte_v6plus_attribs
{
private:
  CLASS const *_this() const { return static_cast<CLASS const *>(this); }
  CLASS *_this() { return static_cast<CLASS *>(this); }

public:
  Unsigned32 _attribs_mask() const
  {
    if (_this()->level == 0)
      return ~Unsigned32(0x0000881c);
    else
      return ~Unsigned32(0x0000022d);
  }

  Unsigned32 _attribs(Page::Attr attr) const
  {
    typedef L4_fpage::Rights R;
    typedef Page::Type T;
    typedef Page::Kern K;

    Mword lower = ATTRIBS::Mp_set_shared;
    if (attr.type == T::Normal())   lower |= ATTRIBS::CACHEABLE;
    if (attr.type == T::Buffered()) lower |= ATTRIBS::BUFFERED;
    if (attr.type == T::Uncached()) lower |= ATTRIBS::NONCACHEABLE;
    Mword upper = lower & ~0x0f;
    upper |= 0x10;  // AP[0]
    lower &= 0x0f;

    if (!(attr.kern & K::Global()))
      upper |= 0x800;

    if (!(attr.rights & R::W()))
      upper |= 0x200;

    if (attr.rights & R::U())
      upper |= 0x20;

    if (!(attr.rights & R::X()))
      {
        if (_this()->level == 0)
          lower |= 0x10;
        else
          lower |= 0x01;
      }

    if (_this()->level == 0)
      return lower | (upper << 6);
    else
      return lower | upper;
  }

  Page::Attr attribs() const
  {
    typedef L4_fpage::Rights R;
    typedef Page::Type T;
    typedef Page::Kern K;

    auto c = access_once(_this()->pte);

    R rights = R::R();

    if (_this()->level == 0)
      {
        if (!(c & 0x10))
          rights |= R::X();

        c = (c & 0x0f) | ((c >> 6) & 0xfff0);
      }
    else if (!(c & 0x01))
      rights |= R::X();

    if (!(c & 0x200))
      rights |= R::W();
    if (c & 0x20)
      rights |= R::U();

    T type;
    switch (c & ATTRIBS::Cache_mask)
      {
      default:
      case ATTRIBS::CACHEABLE: type = T::Normal(); break;
      case ATTRIBS::BUFFERED:  type = T::Buffered(); break;
      case ATTRIBS::NONCACHEABLE: type = T::Uncached(); break;
      }

    K k(0);
    if (!(c & 0x800))
      k |= K::Global();

    return Page::Attr(rights, type, k);
  }

  Unsigned32 _page_bits() const { return 2; }

  Page::Rights access_flags() const
  { return Page::Rights(0); }

  void del_rights(L4_fpage::Rights r)
  {
    Mword n_attr = 0;
    if (r & L4_fpage::Rights::W())
      {
        if (_this()->level == 0)
          n_attr = 0x200 << 6;
        else
          n_attr = 0x200;
      }

    if (r & L4_fpage::Rights::X())
      {
        if (_this()->level == 0)
          n_attr |= 0x10;
        else
          n_attr |= 0x01;
      }

    if (!n_attr)
      return;

    auto p = access_once(_this()->pte);
    if ((p & n_attr) != n_attr)
      {
        p |= n_attr;
        write_now(_this()->pte, p);
      }
  }
};


/**
 * Mixin for PTE pointers for ARMv6+ LPAE page-table attributes
 * (long descriptors).
 */
template<typename CLASS, typename ATTRIBS>
class Pte_long_attribs
{
private:
  CLASS const *_this() const { return static_cast<CLASS const *>(this); }
  CLASS *_this() { return static_cast<CLASS *>(this); }

public:
  Unsigned64 _attribs_mask() const
  { return ~Unsigned64(0x00400000000008dc); }

  Unsigned64 _attribs(Page::Attr attr) const
  {
    typedef L4_fpage::Rights R;
    typedef Page::Type T;
    typedef Page::Kern K;

    Unsigned64 lower = 0x300; // inner sharable
    if (attr.type == T::Normal())   lower |= ATTRIBS::CACHEABLE;
    if (attr.type == T::Buffered()) lower |= ATTRIBS::BUFFERED;
    if (attr.type == T::Uncached()) lower |= ATTRIBS::NONCACHEABLE;

    if (!(attr.kern & K::Global()))
      lower |= 0x800;

    if (!(attr.rights & R::W()))
      lower |= 0x080;

    if (attr.rights & R::U())
      lower |= 0x040;

    if (!(attr.rights & R::X()))
      lower |= 0x0040000000000000;

    return lower;
  }

  Page::Attr attribs() const
  {
    typedef L4_fpage::Rights R;
    typedef Page::Type T;
    typedef Page::Kern K;

    auto c = access_once(_this()->pte);

    R rights = R::R();
    if (!(c & 0x80))
      rights |= R::W();
    if (c & 0x40)
      rights |= R::U();

    if (!(c & 0x0040000000000000))
      rights |= R::X();

    T type;
    switch (c & ATTRIBS::Cache_mask)
      {
      default:
      case ATTRIBS::CACHEABLE: type = T::Normal(); break;
      case ATTRIBS::BUFFERED:  type = T::Buffered(); break;
      case ATTRIBS::NONCACHEABLE: type = T::Uncached(); break;
      }

    K k(0);
    if (!(c & 0x800))
      k |= K::Global();

    return Page::Attr(rights, type, k);
  }

  Unsigned64 _page_bits() const
  {
    return 0x400 | ((_this()->level == CLASS::Max_level) ? 3 : 1);
  }

  Page::Rights access_flags() const
  { return Page::Rights(0); }

  void del_rights(L4_fpage::Rights r)
  {
    Unsigned64 n_attr = 0;
    if (r & L4_fpage::Rights::W())
      n_attr = 0x80;

    if (r & L4_fpage::Rights::X())
      n_attr |= 0x0040000000000000;

    if (!n_attr)
      return;

    auto p = access_once(_this()->pte);
    if ((p & n_attr) != n_attr)
      {
        p |= n_attr;
        write_now(_this()->pte, p);
      }
  }
};


/**
 * Mixin for PTE pointers for ARMv6+ stage 2 page-table attributes
 */
template<typename CLASS, typename ATTRIBS>
class Pte_stage2_attribs
{
private:
  CLASS const *_this() const { return static_cast<CLASS const *>(this); }
  CLASS *_this() { return static_cast<CLASS *>(this); }

public:
  Unsigned64 _attribs_mask() const
  { return ~Unsigned64(0x00400000000000fc); }

  Unsigned64 _attribs(Page::Attr attr) const
  {
    typedef L4_fpage::Rights R;
    typedef Page::Type T;

    Unsigned64 lower = 0x300 | (0x1 << 6); // inner sharable, readable
    if (attr.type == T::Normal())   lower |= ATTRIBS::CACHEABLE;
    if (attr.type == T::Buffered()) lower |= ATTRIBS::BUFFERED;
    if (attr.type == T::Uncached()) lower |= ATTRIBS::NONCACHEABLE;

    if (attr.rights & R::W())
      lower |= (0x2 << 6);

    if (!(attr.rights & R::X()))
      lower |= 0x0040000000000000;

    return lower;
  }

  Page::Attr attribs() const
  {
    typedef L4_fpage::Rights R;
    typedef Page::Type T;
    typedef Page::Kern K;

    auto c = access_once(_this()->pte);

    R rights = R::R();
    rights |= R::U();

    if (c & (0x2 << 6))
      rights |= R::W();

    if (!(c & 0x0040000000000000))
      rights |= R::X();

    T type;
    switch (c & ATTRIBS::Cache_mask)
      {
      default:
      case ATTRIBS::CACHEABLE:    type = T::Normal(); break;
      case ATTRIBS::BUFFERED:     type = T::Buffered(); break;
      case ATTRIBS::NONCACHEABLE: type = T::Uncached(); break;
      }

    return Page::Attr(rights, type, K(0));
  }

  Unsigned64 _page_bits() const
  {
    return 0x400 | ((_this()->level == CLASS::Max_level) ? 3 : 1);
  }

  Page::Rights access_flags() const
  { return Page::Rights(0); }

  void del_rights(L4_fpage::Rights r)
  {
    Unsigned64 n_attr = 0;
    Mword a_attr = 0;
    if (r & L4_fpage::Rights::W())
      a_attr = 0x2 << 6;

    if (r & L4_fpage::Rights::X())
      n_attr |= 0x0040000000000000;

    if (!n_attr && !a_attr)
      return;

    auto p = access_once(_this()->pte);
    auto old = p;
    p |= n_attr;
    p &= ~Unsigned64(a_attr);

    if (p != old) {
      write_now(_this()->pte, p); }
  }
};

