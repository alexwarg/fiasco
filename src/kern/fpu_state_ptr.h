#pragma once

class Fpu_state;

class Fpu_state_ptr
{
public:
  Fpu_state_ptr() = default;
  Fpu_state_ptr(Fpu_state_ptr const &) = delete;
  Fpu_state_ptr &operator = (Fpu_state_ptr const &) = delete;

  Fpu_state_ptr(Fpu_state_ptr &&o) : _state(o.reset()) {}
  Fpu_state_ptr &operator = (Fpu_state_ptr &&o)
  {
    if (&o == this)
      return *this;
    _state = o.reset();
    return *this;
  }

  bool valid() const
  { return _state != nullptr; }

  explicit operator bool () const { return valid(); }

  Fpu_state *get() const
  { return _state; }

  Fpu_state &operator * () const
  { return *_state; }

  Fpu_state *reset(Fpu_state *n = nullptr)
  {
    Fpu_state *r = _state;
    _state = n;
    return r;
  }

  void set(Fpu_state *state)
  { _state = state; }

private:
  Fpu_state *_state = nullptr;
};

