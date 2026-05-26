#pragma once

#include <context.h>
#include <thread.h>
#include <cpu.h>
#include <kobject_dbg.h>
#include <mem_layout.h>
#include <types.h>

#include <jdb_tcb_ptr_arch.h>


class Jdb_tcb_ptr : public Jdb_tcb_ptr_arch<Jdb_tcb_ptr>
{
public:
  Jdb_tcb_ptr(Address addr = 0)
    : _base(addr & ~(Context::Size-1)),
      _offs(addr &  (Context::Size-1))
  {}

  inline bool valid() const
  { return _offs <= Context::Size-sizeof(Mword); }

  bool operator > (int offs) const
  {
    return offs < 0 ? _offs > Context::Size + offs*sizeof(Mword)
                    : _offs > offs*sizeof(Mword);
  }

  Jdb_tcb_ptr &operator += (int offs)
  { _offs += offs*sizeof(Mword); return *this; }

  inline Address addr() const
  { return _base + _offs; }

  inline Mword value() const
  { return *(Mword*)(_base + _offs); }

  inline void value(Mword v)
  { *(Mword*)(_base + _offs) = v; }

  inline bool is_user_value() const
  {
    return arch_is_user_value(_offs);
  }

  Space *space(Thread *user_thread) const
  {
    return is_user_value() ? user_thread->space() : 0;
  }

  const char *user_value_desc() const
  {
    return arch_user_value_desc(_offs);
  }

  inline Mword const *top_value_ptr(int offs) const
  { return (Mword*)(Cpu::stack_align(_base + Context::Size)) + offs; }

  inline Mword top_value(int offs) const
  { return *top_value_ptr(offs); }

  inline Address base() const
  { return _base; }

  inline Address offs() const
  { return _offs; }

  inline void offs(Address offs)
  { _offs = offs; }

  inline bool is_kern_code() const
  { return (Address)&Mem_layout::image_start <= value()
           && value() <= (Address)&Mem_layout::ecode;  };

  inline bool is_kobject() const
  { return Kobject_dbg::is_kobj(reinterpret_cast<void *>(value())); }


private:
  Address  _base;
  Address  _offs;
};

