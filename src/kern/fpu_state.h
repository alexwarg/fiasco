#pragma once

class Fpu_state
{
public:
  void *state_buffer() const
  {
    return _state_buffer;
  }

  void state_buffer(void *b)
  {
    _state_buffer = b;
  }

private:
  void *_state_buffer = nullptr;
};

