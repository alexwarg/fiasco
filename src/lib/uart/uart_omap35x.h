/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2009 Technische Universität Dresden.
 * Copyright (C) 2023-2024 Kernkonzept GmbH.
 * Author(s) Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "uart_base.h"

namespace L4
{
  class Uart_omap35x : public Uart
  {
  public:
    explicit Uart_omap35x() {}
    explicit Uart_omap35x(unsigned /*base_rate*/) {}
    Uart_omap35x() = default;
    void set_base_rate(unsigned) override {}
    bool startup(Io_register_block const *) override;
    void shutdown() override;
    bool change_mode(Transfer_mode m, Baud_rate r) override;
    int tx_avail() const;
    void wait_tx_done() const;
    inline void out_char(char c) const;
    int write(char const *s, unsigned long count,
              bool blocking = true) const override;

    bool enable_rx_irq(bool) override;
    int char_avail() const override;
    int get_char(bool blocking = true) const override;
  };
};
