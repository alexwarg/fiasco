#pragma once

#include <io.h>

class Pit
{
  enum
  {
    Clock_tick_rate = 1193182,
  };

public:
  static void done()
  {
    // set counter channel 0 to one-shot mode
    Io::out8_p(0x30, 0x43);
  }

  // set up timer interrupt (~ 1ms)
  static void init()
  {
    // set counter frequency to ~1000 Hz (1000.151 Hz)
    set_freq(1000);
  }

  static void init(unsigned freq)
  {
    set_freq(freq);
  }

  static void set_freq_slow()
  {
    set_freq(32);
  }

  static void setup_channel2_to_20hz()
  {
    // set gate high, disable speaker
    Io::out8((Io::in8(0x61) & ~0x02) | 0x01, 0x61);

    // set counter channel 2 to binary, mode0, lsb/msb
    Io::out8(0xb0, 0x43);

    // set counter frequency
    const unsigned latch = Clock_tick_rate / 20; // 50ms
    Io::out8(latch & 0xff, 0x42);
    Io::out8(latch >> 8,   0x42);
  }

private:
  static void set_freq(unsigned freq)
  {
    // set counter channel 0 to binary, mode2, lsb/msb
    Io::out8_p(0x34, 0x43);

    // set counter frequency
    const unsigned latch = Clock_tick_rate / freq;
    Io::out8_p(latch & 0xff, 0x40);
    Io::out8_p(latch >> 8,   0x40);
  }
};

