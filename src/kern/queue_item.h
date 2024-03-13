#pragma once

#include <cxx/dlist>
#include "assert.h"

class Queue;

class alignas(16) Queue_item : public cxx::D_list_item
{
  friend class Queue;

private:
  Queue *_q;

public:
  bool queued() const noexcept
  {
    return cxx::D_list_cyclic<Queue_item>::in_list(this);
  }

  Queue *queue() const noexcept
  {
    assert (queued());
    return _q;
  }
};

