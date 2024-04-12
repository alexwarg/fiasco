#pragma once

#include <cxx/bitfield>
#include <types.h>

struct Thread_state
{
  Mword state;
  CXX_BITFIELD_MEMBER( 0,  0, ready,      state);
  CXX_BITFIELD_MEMBER( 1,  1, drq_ready,  state);
  CXX_BITFIELD_MEMBER( 0,  1, ready_mask, state);

  CXX_BITFIELD_MEMBER( 2,  2, send_wait,           state);
  CXX_BITFIELD_MEMBER( 3,  3, receive_wait,        state);
  CXX_BITFIELD_MEMBER( 4,  4, receive_in_progress, state);
  CXX_BITFIELD_MEMBER( 2,  4, ipc_mask,            state);
  CXX_BITFIELD_MEMBER( 5,  5, ipc_transfer,        state);
  CXX_BITFIELD_MEMBER( 6,  6, transfer_failed,     state);

  CXX_BITFIELD_MEMBER( 7,  7, cancel,  state);
  CXX_BITFIELD_MEMBER( 8,  8, timeout, state);
  CXX_BITFIELD_MEMBER( 2,  8, full_ipc_mask, state);

  CXX_BITFIELD_MEMBER( 9,  9, dead,  state);
  CXX_BITFIELD_MEMBER(10, 10, dying, state);

  CXX_BITFIELD_MEMBER(12, 12, finish_migration, state);
  CXX_BITFIELD_MEMBER(13, 13, need_resched,     state);
  CXX_BITFIELD_MEMBER(12, 13, switch_hazards,   state);

  CXX_BITFIELD_MEMBER(15, 15, fpu_owner, state);
  CXX_BITFIELD_MEMBER(16, 16, alien, state);
  CXX_BITFIELD_MEMBER(17, 17, dis_alien, state);
  CXX_BITFIELD_MEMBER(18, 18, in_exception, state);

  CXX_BITFIELD_MEMBER(20, 20, drq_wait, state);
  CXX_BITFIELD_MEMBER(21, 21, waiting, state);

  CXX_BITFIELD_MEMBER(22, 22, vcpu_enabled, state);
  CXX_BITFIELD_MEMBER(22, 22, vcpu, state);

  CXX_BITFIELD_MEMBER(23, 23, vcpu_user, state);
  CXX_BITFIELD_MEMBER(24, 24, vcpu_fpu_disabled, state);
  CXX_BITFIELD_MEMBER(25, 25, vcpu_ext_enabled, state);
  CXX_BITFIELD_MEMBER(25, 25, vcpu_ext, state);

  CXX_BITFIELD_MEMBER(22, 25, vcpu_state_mask, state);

  CXX_BITFIELD_MEMBER(26, 26, arch_flag, state);

  Thread_state() = default;
  constexpr Thread_state(Mword s) : state(s) {}
};

enum Thread_state_
{
  Thread_invalid          = 0,	  // tcb unallocated
  Thread_ready            = 0x1,  // can be scheduled
  Thread_drq_ready        = 0x2,
  Thread_ready_mask       = Thread_ready | Thread_drq_ready,

  Thread_send_wait           = 0x4,  // waiting to send a message
  Thread_receive_wait        = 0x8,  // waiting for a message
  Thread_receive_in_progress = 0x10, // receiving a message (passively)

  Thread_ipc_mask            = Thread_send_wait | Thread_receive_wait
                             | Thread_receive_in_progress,

  Thread_ipc_transfer        = 0x20,

  // the ipc handshake bits needs to be in the first byte,
  // the shortcut depends on it

  Thread_transfer_failed      = 0x40, ///< The IPC operation is canceled by the receiver
  Thread_cancel               = 0x80, // state has been changed -- cancel activity
  Thread_timeout              = 0x100,

  Thread_full_ipc_mask        = Thread_ipc_mask | Thread_cancel | Thread_transfer_failed
                                | Thread_timeout | Thread_ipc_transfer,

  Thread_ipc_abort_mask       = Thread_transfer_failed | Thread_cancel | Thread_timeout
                                | Thread_ipc_transfer,

  Thread_dead                 = 0x200,// tcb allocated, but inactive (not in any q)
  Thread_dying                = 0x400,// thread is about to be killed

  // 0x800 is free

  Thread_finish_migration     = 0x1000,
  Thread_need_resched         = 0x2000,
  Thread_switch_hazards       = Thread_finish_migration | Thread_need_resched,

  // 0x4000 is free

  Thread_fpu_owner            = 0x8000, // currently owns the fpu
  Thread_alien                = 0x10000, // Thread is an alien, is not allowed
                                     // to do system calls
  Thread_dis_alien            = 0x20000, // Thread is an alien, however the next
                                     // system call is allowed
  Thread_in_exception         = 0x40000, // Thread has sent an exception but still
                                     // got no reply

  // 0x80000 is free

  Thread_drq_wait             = 0x100000, // Thread polls for DRQs
  Thread_waiting              = 0x200000, // Thread waits for a lock

  Thread_vcpu_enabled         = 0x400000,
  Thread_vcpu_user            = 0x800000,
  Thread_vcpu_fpu_disabled    = 0x1000000,
  Thread_ext_vcpu_enabled     = 0x2000000,
  // MIPS uses bit 0x4000000

  Thread_vcpu_state_mask      = Thread_vcpu_enabled | Thread_vcpu_user
                                | Thread_vcpu_fpu_disabled
                                | Thread_ext_vcpu_enabled
};
