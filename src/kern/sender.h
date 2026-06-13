#pragma once

#include "l4_types.h"
#include "prio_list.h"
#include "cpu_lock.h"
#include "lock_guard.h"

#include <cassert>

class Receiver;

/** A sender.  This is a role class, so real senders need to inherit from it.
 */
class Sender : private Prio_list_elem
{
  MEMBER_OFFSET();
public:

  using Prio_list_elem::wait_queue;

  /** Receiver-ready callback. Receivers call this function on waiting senders
      when they get ready to receive a message from that sender. Senders need
      to implement this interface. */
  virtual void ipc_send_msg(Receiver *, bool open_wait) = 0;
  virtual void ipc_receiver_aborted() = 0;
  virtual void modify_label(Mword const *todo, int cnt) = 0;

  unsigned short sender_prio() const
  {
    return Prio_list_elem::prio();
  }

  /** Sender in a queue of senders?.
      @return true if sender has enqueued in a receiver's list of waiting
              senders
   */
  bool in_sender_list() const
  {
    return Prio_list_elem::in_list();
  }

  bool is_head_of(Prio_list const *l) const
  {
    return l->first() == this;
  }

  static Sender *cast(Prio_list_elem *e)
  {
    return static_cast<Sender*>(e);
  }

  bool sender_enqueue(Prio_list *head, unsigned short prio)
  {
    assert(prio < 256);

    auto guard = lock_guard(cpu_lock);
    return head->insert(this, prio);
  }

  template< typename P_LIST >
  bool sender_dequeue(P_LIST list)
  {
    if (!in_sender_list())
      return false;

    auto guard = lock_guard(cpu_lock);
    return list->dequeue(this);
  }

  Prio_list_elem *qitem() { return this; }

protected:
  Sender() = default;

private:
  friend class Jdb;
  friend class Jdb_thread_list;
};

