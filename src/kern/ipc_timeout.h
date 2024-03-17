#pragma once

#include "timeout.h"
#include "receiver.h"
#include "context.h"

class IPC_timeout : public Timeout
{
  friend class Jdb_list_timeouts;

public:
  IPC_timeout() = default;

  /**
   * IPC_timeout destructor
   */
  ~IPC_timeout() override
  {
    owner()->set_timeout (0);	// reset owner's timeout field
  }

  Receiver *owner() const noexcept
  {
    // We could have saved our context in our constructor, but computing
    // it this way is easier and saves space. We can do this as we know
    // that IPC_timeouts are always created on the kernel stack of the
    // owner context.

    return reinterpret_cast<Receiver *>(context_of (this));
  }

private:
  bool expired() override;
};
