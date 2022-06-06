#pragma once

#include <entry_frame.h>
#include <trap_state.h>
#include <vcpu_host_regs.h>
#include <cxx/atomic>

class Vcpu_state
{
  MEMBER_OFFSET();
public:
  enum State : Unsigned16
  {
    F_irqs        = 0x1,
    F_page_faults = 0x2,
    F_exceptions  = 0x4,
    F_user_mode   = 0x20,
    F_fpu_enabled = 0x80,
    F_traps       = F_irqs | F_page_faults | F_exceptions,
  };

  enum Sticky_flags : Unsigned16
  {
    Sf_irq_pending = 0x01,
  };

  /// vCPU ABI version (must be checked by the user for equality).
  Mword version = Vcpu_arch_version;
  /// user-specific data
  Mword user_data[7];

  Trex _regs;
  Syscall_frame _ipc_regs;

  cxx::atomic<Unsigned16> _state{0};
  cxx::atomic<Unsigned16> _saved_state{0};
  cxx::atomic<Unsigned16> _sticky_flags{0};
  Unsigned16 _reserved = 0;

  L4_obj_ref user_task;

  Mword _entry_sp = 0;
  Mword _entry_ip = 0;

  // kernel-internal private state
  Mword _sp = 0;
  Vcpu_host_regs host;

  Unsigned16 save_state_disable(Unsigned16 del)
  {
    Unsigned16 s = _state.fetch_and(~del, cxx::memory_order_relaxed);
    _saved_state.store(s, cxx::memory_order_relaxed);
    return s;
  }

  Unsigned16 restore_state()
  {
    Unsigned16 s = _saved_state.load(cxx::memory_order_relaxed);
    _state.store(s, cxx::memory_order_relaxed);
    return s;
  }

  Mword disable_irqs()
  {
    return _state.fetch_and(~F_irqs, cxx::memory_order_relaxed)
      & F_irqs;
  }

  void restore_irqs(Unsigned16 irqs)
  {
    if (irqs & F_irqs)
      _state.or_fetch(F_irqs, cxx::memory_order_relaxed);
  }

  void set_irq_pending()
  {
    _sticky_flags.or_fetch(Sf_irq_pending, cxx::memory_order_relaxed);
  }

  void clear_irq_pending()
  {
    _sticky_flags.and_fetch(~Sf_irq_pending, cxx::memory_order_relaxed);
  }

  bool irqs_enabled() const
  {
    return _state.load(cxx::memory_order_relaxed) & F_irqs;
  }

  bool pf_enabled() const
  {
    return _state.load(cxx::memory_order_relaxed) & F_page_faults;
  }

  bool exc_enabled() const
  {
    return _state.load(cxx::memory_order_relaxed) & F_exceptions;
  }

  bool user_mode() const
  {
    return _state.load(cxx::memory_order_relaxed) & F_user_mode;
  }

  bool saved_user_mode() const
  {
    return _saved_state.load(cxx::memory_order_relaxed) & F_user_mode;
  }

  Unsigned16 saved_state() const
  {
    return _saved_state.load(cxx::memory_order_relaxed);
  }

  Unsigned16 state() const
  {
    return _state.load(cxx::memory_order_relaxed);
  }

  bool saved_irqs_enabled() const
  {
    return _saved_state.load(cxx::memory_order_relaxed) & F_irqs;
  }

  bool pending_irqs() const
  {
    return _sticky_flags.load(cxx::memory_order_relaxed) & Sf_irq_pending;
  }

  Unsigned16 kern_entry_state()
  {
    return _state.fetch_and(~(F_traps | F_user_mode), cxx::memory_order_relaxed);
  }

  Unsigned16 user_entry_state()
  {
    return _state.fetch_or(F_traps, cxx::memory_order_relaxed);
  }

  template<typename REGS>
  void kern_entry(REGS regs)
  {
    Mword flags = Vcpu_state::F_traps
                | Vcpu_state::F_user_mode;
    if (save_state_disable(flags) & F_user_mode)
      _sp = _entry_sp;
    else
      _sp = regs->sp();
  }
};
