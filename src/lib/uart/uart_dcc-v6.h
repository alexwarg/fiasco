/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2009 Technische Universität Dresden.
 * Copyright (C) 2023-2024 Kernkonzept GmbH.
 * Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */
#pragma once

#include "uart_base.h"

namespace L4
{
  class Uart_dcc_v6 : public Uart
  {
  public:
    explicit Uart_dcc_v6() {}
    explicit Uart_dcc_v6(unsigned /*base_rate*/) {}
    Uart_dcc_v6() = default;
    void set_base_rate(unsigned) override {}
    bool startup(Io_register_block const *) override;
    void shutdown() override;
    bool change_mode(Transfer_mode m, Baud_rate r) override;
    int get_char(bool blocking = true) const override;
    int char_avail() const override;
    int tx_avail() const;
    void wait_tx_done() const;
    inline void out_char(char c) const;
    int write(char const *s, unsigned long count,
              bool blocking = true) const override;
  private:
    unsigned get_status() const;
  };
};
