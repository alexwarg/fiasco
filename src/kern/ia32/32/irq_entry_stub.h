#pragma once

class Irq_base;

/** this structure must exactly map to the code stubs from 32/entry.S */
struct Irq_entry_stub
{
  char _res[2];
  Irq_base *irq;
  char _res2[5];
} __attribute__((packed));

