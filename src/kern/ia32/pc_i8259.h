
#pragma once

#include <io.h>
#include <i8259.h>

struct Pc_i8259 : Irq_i8259_base<Io>
{
  constexpr Pc_i8259() : Irq_i8259_base<Io>(0x20, 0xa0) {}
};
