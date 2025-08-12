/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2016 Technische Universität Dresden.
 * Copyright (C) 2023-2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@l4re.org>
 *
 * This file is part of L4Re and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "uart_base.h"

namespace L4 {

class Uart_sh : public Uart
{
public:
  explicit Uart_sh() {}
  explicit Uart_sh(unsigned /*base_rate*/) {}
  Uart_sh() = default;
  void set_base_rate(unsigned) override {}
  bool startup(Io_register_block const *) override;
  void shutdown() override;
  bool change_mode(Transfer_mode m, Baud_rate r) override;
  int tx_avail() const;
  void wait_tx_done() const {}
  inline void out_char(char c) const;
  int write(char const *s, unsigned long count,
            bool blocking = true) const override;

  bool enable_rx_irq(bool enable = true) override;
  void irq_ack() override;
  int char_avail() const override;
  int get_char(bool blocking = true) const override;
};

}
