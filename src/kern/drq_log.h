#pragma once

#include <tb_entry.h>
#include <string_buffer.h>
#include <drq.h>

struct Drq_log : public Tb_entry
{
  void *func;
  Context *thread;
  Drq const *rq;
  Cpu_number target_cpu;
  enum class Type
  {
    Send, Do_send, Do_request, Send_reply, Do_reply, Done
  } type;
  bool wait;
  void print(String_buffer *buf) const;
  Group_order has_partner() const
  {
    switch (type)
      {
      case Type::Send: return Group_order::first();
      case Type::Do_send: return Group_order(1);
      case Type::Do_request: return Group_order(2);
      case Type::Send_reply: return Group_order(3);
      case Type::Do_reply: return Group_order(4);
      case Type::Done: return Group_order::last();
      }
    return Group_order::none();
  }

  Group_order is_partner(Drq_log const *o) const
  {
    if (rq != o->rq || func != o->func)
      return Group_order::none();

    return o->has_partner();
  }
};

