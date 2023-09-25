#pragma once

#include "l4_error.h"
#include "l4_types.h"

#include "globals.h"
#include "kip.h"
#include "entry_frame.h"

enum Tbuf_entry_fixed
{
  Tbuf_unused             = 0,
  Tbuf_pf,
  Tbuf_ipc,
  Tbuf_ipc_res,
  Tbuf_ipc_trace,
  Tbuf_ke,
  Tbuf_ke_reg,
  Tbuf_breakpoint,
  Tbuf_ke_bin,
  Tbuf_dynentries,

  Tbuf_max                = 0x80,
  Tbuf_hidden             = 0x80,
};

class Tb_entry;
class Context;
class Space;
class Sched_context;
class Syscall_frame;
class Trap_state;
class Tb_entry_formatter;
class String_buffer;

struct Tb_log_table_entry
{
  char const *name;
  unsigned char *patch;
  Tb_entry_formatter *fmt;
};

extern Tb_log_table_entry _jdb_log_table[];
extern Tb_log_table_entry _jdb_log_table_end;



class Tb_entry : public Tb_entry_arch
{
protected:
  Mword         _number;        ///< event number
  Address       _ip;            ///< instruction pointer
  Unsigned64    _tsc;           ///< time stamp counter
  Context const *_ctx;          ///< Context
  Unsigned32    _pmc1;          ///< performance counter value 1
  Unsigned32    _pmc2;          ///< performance counter value 2
  Unsigned32    _kclock;        ///< lower 32 bits of kernel clock
  Unsigned8     _type;          ///< type of entry
  Unsigned8     _cpu;           ///< CPU

  static Mword (*rdcnt1)();
  static Mword (*rdcnt2)();

  static Mword dummy_read_pmc() { return 0; }

public:
  class Group_order
  {
  public:
    Group_order() : _o(0) {} // not grouped
    Group_order(unsigned v) : _o(2 + v) {}
    static Group_order none() { return Group_order(); }
    static Group_order last() { return Group_order(255, true); }
    static Group_order first() { return Group_order(0); }
    static Group_order direct() { return Group_order(1, true); }

    bool not_grouped() const { return _o == 0; }
    bool is_direct() const { return _o == 1; }
    bool is_first() const { return _o == 2; }
    bool is_last() const { return _o == 255; }
    bool grouped() const { return _o >= 2; }
    unsigned char depth() const { return _o - 2; }

  private:
    Group_order(unsigned char v, bool) : _o(v) {}
    unsigned char _o;
  };

  Group_order has_partner() const { return Group_order::none(); }
  Group_order is_partner(Tb_entry const *) const { return Group_order::none(); }
  Mword partner() const { return 0; }

  static void set_rdcnt(int num, Mword (*f)());
  static void set_cycle_read_func(Unsigned64 (*f)());

  void rdtsc()
  { _tsc = read_cycle_counter(); }

  void clear()
  {
    _type = Tbuf_unused;
  }

  void set_global(char type, Context const *ctx, Address ip)
  {
    _type   = type;
    _ctx    = ctx;
    _ip     = ip;
    _kclock = (Unsigned32)Kip::k()->clock();
    _cpu    = cxx::int_value<Cpu_number>(current_cpu());
  }

  void hide()
  { _type |= Tbuf_hidden; }

  void unhide()
  { _type &= ~Tbuf_hidden; }

  Address ip() const
  { return _ip; }

  Context const *ctx() const
  { return _ctx; }

  Unsigned8 type() const
  { return _type & (Tbuf_max-1); }

  int hidden() const
  { return _type & Tbuf_hidden; }

  Mword number() const
  { return _number; }

  void number(Mword number)
  { _number = number; }

  void rdpmc1()
  { _pmc1 = rdcnt1(); }

  void rdpmc2()
  { _pmc2 = rdcnt2(); }

  Unsigned32 kclock() const
  { return _kclock; }

  Unsigned8 cpu() const
  { return _cpu; }

  Unsigned64 tsc() const
  { return _tsc; }

  Unsigned32 pmc1() const
  { return _pmc1; }

  Unsigned32 pmc2() const
  { return _pmc2; }

} __attribute__((__packed__, __aligned__(8)));


class Tb_entry_union : public Tb_entry
{
private:
  char _padding[Tb_entry_size - sizeof(Tb_entry)];
};

static_assert(sizeof(Tb_entry_union) == Tb_entry::Tb_entry_size,
              "Tb_entry_union has the wrong size");

struct Tb_entry_empty : public Tb_entry
{
  void print(String_buffer *) const {}
};

class Tb_entry_formatter
{
public:
  typedef Tb_entry::Group_order Group_order;

  virtual void print(String_buffer *, Tb_entry const *e) const = 0;
  virtual Group_order has_partner(Tb_entry const *e) const = 0;
  virtual Group_order is_pair(Tb_entry const *e, Tb_entry const *n) const = 0;
  virtual Mword partner(Tb_entry const *e) const = 0;

  static Tb_entry_formatter const *get_fmt(Tb_entry const *e)
  {
    if (e->type() >= Tbuf_dynentries)
      return _jdb_log_table[e->type() - Tbuf_dynentries].fmt;

    return _fixed[e->type()];
  }

  static void set_fixed(unsigned type, Tb_entry_formatter const *f);


private:
  static Tb_entry_formatter const *_fixed[];
};


template< typename T >
class Tb_entry_formatter_t : public Tb_entry_formatter
{
public:
  Tb_entry_formatter_t() {}

  typedef T const *Const_ptr;
  typedef T *Ptr;

  void print(String_buffer *buf, Tb_entry const *e) const override
  { static_cast<Const_ptr>(e)->print(buf); }

  Group_order has_partner(Tb_entry const *e) const override
  { return static_cast<Const_ptr>(e)->has_partner(); }

  Group_order is_pair(Tb_entry const *e, Tb_entry const *n) const override
  {
    //assert (get_fmt(e) == &singleton);

    if (&singleton == get_fmt(n))
      return static_cast<Const_ptr>(e)->is_partner(static_cast<Const_ptr>(n));
    return Tb_entry::Group_order::none();
  }

  Mword partner(Tb_entry const *e) const override
  { return static_cast<Const_ptr>(e)->partner(); }

  static Tb_entry_formatter_t const singleton;
};

template<typename T>
Tb_entry_formatter_t<T> const Tb_entry_formatter_t<T>::singleton;


/** logged ipc. */
class Tb_entry_ipc : public Tb_entry
{
private:
  L4_msg_tag    _tag;           ///< message tag
  Mword         _dword[2];      ///< first two message words
  L4_obj_ref    _dst;           ///< destination id
  Mword         _dbg_id;
  Mword         _label;
  L4_timeout_pair _timeout;     ///< timeout
#ifdef CONFIG_BIT64
  Unsigned64  _to_abs_rcv;      ///< absolute receive timeout
#endif

public:
  Tb_entry_ipc() : _timeout(0) {}
  void print(String_buffer *buf) const;

  void set(Context const *ctx, Mword ip, Syscall_frame *ipc_regs, Utcb *utcb,
           Mword dbg_id, Unsigned64 left)
  {
    (void)left;
    set_global(Tbuf_ipc, ctx, ip);
    _dst       = ipc_regs->ref();
    _label     = ipc_regs->from_spec();


    _dbg_id = dbg_id;

    _timeout   = ipc_regs->timeout();
    _tag       = ipc_regs->tag();
    // hint for gcc
    Mword tmp0 = utcb->values[0];
    Mword tmp1 = utcb->values[1];
    _dword[0]  = tmp0;
    _dword[1]  = tmp1;
  }

  Mword ipc_type() const
  { return _dst.op(); }

  Mword dbg_id() const
  { return _dbg_id; }

  L4_obj_ref dst() const
  { return _dst; }

  L4_timeout_pair timeout() const
  { return _timeout; }

  L4_msg_tag tag() const
  { return _tag; }

  Mword label() const
  { return _label; }

  Mword dword(unsigned index) const
  { return _dword[index]; }

#ifdef CONFIG_BIT64
  void set_abs_timeout(Utcb *utcb)
  {
    if (_timeout.rcv.is_absolute())
      _to_abs_rcv = _timeout.rcv.microsecs_abs(utcb);
  }

  Unsigned64 timeout_abs_rcv() const
  { return _to_abs_rcv; }
#else
  void set_abs_timeout(Utcb *)
  {
    // ignore absolute timeouts due to lack of space
  }

  Unsigned64 timeout_abs_rcv() const
  { return 0ULL; }
#endif
};

/** logged ipc result. */
class Tb_entry_ipc_res : public Tb_entry
{
private:
  L4_msg_tag    _tag;           ///< message tag
  Mword         _dword[2];      ///< first two dwords
  L4_error      _result;        ///< result
  Mword         _from;          ///< receive descriptor
  L4_obj_ref    _dst;           ///< destination id
  Mword         _pair_event;    ///< referred event
  Unsigned8     _have_snd;      ///< IPC had send part
  Unsigned8     _is_np;         ///< next period bit set
public:
  void print(String_buffer *buf) const;

  void set(Context const *ctx, Mword ip, Syscall_frame *ipc_regs,
           Utcb *utcb, Mword result, Mword pair_event, Unsigned8 have_snd,
           Unsigned8 is_np)
  {
    set_global(Tbuf_ipc_res, ctx, ip);
    // hint for gcc
    Mword tmp0 = utcb->values[0];
    Mword tmp1 = utcb->values[1];
    _dword[0]   = tmp0;
    _dword[1]   = tmp1;
    _tag        = ipc_regs->tag();
    _pair_event = pair_event;
    _result     = L4_error::from_raw(result);
    _from       = ipc_regs->from_spec();
    _dst        = ipc_regs->ref();
    _have_snd   = have_snd;
    _is_np      = is_np;
  }

  int have_snd() const
  { return _have_snd; }

  int is_np() const
  { return _is_np; }

  Mword from() const
  { return _from; }

  L4_error result() const
  { return _result; }

  L4_msg_tag tag() const
  { return _tag; }

  Mword dword(unsigned index) const
  { return _dword[index]; }

  Mword pair_event() const
  { return _pair_event; }

  bool ipc_has_recv_phase() const
  { return !!(_dst.op() & L4_obj_ref::Ipc_recv); }
};


/** logged pagefault. */
class Tb_entry_pf : public Tb_entry
{
private:
  Address       _pfa;           ///< pagefault address
  Mword         _error;         ///< pagefault error code
  Space         *_space;
public:
  // Unused because PF logging type < Tbuf_dynentries, see formatter_default()
  void print(String_buffer *) const {}

  void set(Context const *ctx, Address ip, Address pfa,
           Mword error, Space *spc)
  {
    set_global(Tbuf_pf, ctx, ip);
    _pfa   = pfa;
    _error = error;
    _space = spc;
  }

  Mword error() const
  { return _error; }

  Address pfa() const
  { return _pfa; }

  Space *space() const
  { return _space; }
};

/** logged kernel event. */
template<unsigned BASE_SIZE>
union Tb_entry_msg
{
  char msg[Tb_entry::Tb_entry_size - BASE_SIZE];
  struct Ptr
  {
    char tag[2];
    char const *ptr;
  } mptr;

  void set_const(char const *msg)
  {
    mptr.tag[0] = 0;
    mptr.tag[1] = 1;
    mptr.ptr = msg;
  }

  void set_buf(unsigned i, char c)
  {
    if (i < sizeof(msg) - 1)
      msg[i] = c >= ' ' ? c : '.';
  }

  void term_buf(unsigned i)
  {
    msg[i < sizeof(msg) - 1 ? i : sizeof(msg) - 1] = '\0';
  }

  char const *str() const
  {
    return mptr.tag[0] == 0 && mptr.tag[1] == 1 ? mptr.ptr : msg;
  }
};

class Tb_entry_ke : public Tb_entry
{
public:
  Tb_entry_msg<sizeof(Tb_entry)> msg;
  void set(Context const *ctx, Address ip)
  { set_global(Tbuf_ke, ctx, ip); }
};

class Tb_entry_ke_reg : public Tb_entry
{
public:
  Mword v[3];
  Tb_entry_msg<sizeof(Tb_entry) + sizeof(v)> msg;
  void set(Context const *ctx, Address ip)
  { set_global(Tbuf_ke_reg, ctx, ip); }
};

/** logged breakpoint. */
class Tb_entry_bp : public Tb_entry
{
private:
  Address       _address;       ///< breakpoint address
  int           _len;           ///< breakpoint length
  Mword         _value;         ///< value at address
  int           _mode;          ///< breakpoint mode
public:
  void print(String_buffer *buf) const;

  void set(Context const *ctx, Address ip,
           int mode, int len, Mword value, Address address)
  {
    set_global(Tbuf_breakpoint, ctx, ip);
    _mode    = mode;
    _len     = len;
    _value   = value;
    _address = address;
  }

  int mode() const
  { return _mode; }

  int len() const
  { return _len; }

  Mword value() const
  { return _value; }

  Address addr() const
  { return _address; }
};

/** logged binary kernel event. */
class Tb_entry_ke_bin : public Tb_entry
{
public:
  char _msg[Tb_entry_size - sizeof(Tb_entry)];
  enum { SIZE = 30 };

  void set(Context const *ctx, Address ip)
  { set_global(Tbuf_ke_bin, ctx, ip); }

  void set_buf(unsigned i, char c)
  {
    if (i < sizeof(_msg)-1)
      _msg[i] = c;
  }
};

