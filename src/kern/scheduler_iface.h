#pragma once

class Scheduler_iface
{
private:
  static Scheduler_iface *_root;

protected:
  explicit Scheduler_iface(bool) { _root = this; }

public:
  static Scheduler_iface *root() { return _root; }

  Scheduler_iface() = default;
  virtual ~Scheduler_iface() = default;

  virtual void trigger_hotplug_event() = 0;
};
