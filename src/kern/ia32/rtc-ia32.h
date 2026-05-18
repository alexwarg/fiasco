#pragma once

#include <io.h>
#include <globalconfig.h>

class Rtc
{
public:
  Rtc() = delete;
  Rtc(const Rtc&) = delete;

  enum Regs
  {
    RTC_STATUSA         = 0x0a, /* status register A */
     RTCSA_TUP          = 0x80, /* time update, don't look now */
     RTCSA_DIVIDER      = 0x20, /* divider correct for 32768 Hz */
     RTCSA_8192         = 0x03,
     RTCSA_4096         = 0x04,
     RTCSA_2048         = 0x05,
     RTCSA_1024         = 0x06,
     RTCSA_512          = 0x07,
     RTCSA_256          = 0x08,
     RTCSA_128          = 0x09,
     RTCSA_64           = 0x0a,
     RTCSA_32           = 0x0b,

    RTC_STATUSB         = 0x0b, /* status register B */
     RTCSB_DST          = 0x01, /* Daylight Savings Time enable	*/
     RTCSB_24HR         = 0x02, /* 0 = 12 hours, 1 = 24	hours */
     RTCSB_BCD          = 0x04, /* 0 = BCD, 1 =	Binary coded time */
     RTCSB_SQWE         = 0x08, /* 1 = output sqare wave at SQW	pin */
     RTCSB_UINTR        = 0x10, /* 1 = enable update-ended interrupt */
     RTCSB_AINTR        = 0x20, /* 1 = enable alarm interrupt */
     RTCSB_PINTR        = 0x40, /* 1 = enable periodic clock interrupt */
     RTCSB_HALT         = 0x80, /* stop clock updates */

    RTC_INTR            = 0x0c, /* status register C (R) interrupt source */
     RTCIR_UPDATE       = 0x10, /* update intr */
     RTCIR_ALARM        = 0x20, /* alarm intr */
     RTCIR_PERIOD       = 0x40, /* periodic intr */
     RTCIR_INT          = 0x80, /* interrupt output signal */
  };

  // set up timer interrupt (~ 1ms)
  static
  void init()
  {
    while (reg_read(RTC_STATUSA) & RTCSA_TUP) 
      ; // wait till RTC ready

#ifdef CONFIG_SLOW_RTC
    // set divider to 64 Hz
    reg_write(RTC_STATUSA, RTCSA_DIVIDER | RTCSA_64);
#else
    // set divider to 1024 Hz
    reg_write(RTC_STATUSA, RTCSA_DIVIDER | RTCSA_1024);
#endif

    // set up interrupt
    reg_write(RTC_STATUSB, reg_read(RTC_STATUSB) | RTCSB_PINTR | RTCSB_SQWE); 

    // reset
    reg_read(RTC_INTR);
  }

  static
  void done()
  {
    // disable all potential interrupt sources
    reg_write(RTC_STATUSB,
              reg_read(RTC_STATUSB) & ~(RTCSB_PINTR | RTCSB_AINTR | RTCSB_UINTR));

    // reset
    reg_read(RTC_INTR);
  }

  static
  void set_freq_slow()
  {
    // set divider to 32 Hz
    reg_write(RTC_STATUSA, RTCSA_DIVIDER | RTCSA_32);
  }

  static
  void set_freq_normal()
  {
    // set divider to 1024 Hz
    reg_write(RTC_STATUSA, RTCSA_DIVIDER | RTCSA_1024);
  }

  // acknowledge RTC interrupt
  static
  void reset()
  {
    // reset irq by reading the cmos port
    // do it fast because we are cli'd
    asm volatile ("movb $0xc, %%al\n\t"
                  "outb %%al,$0x70\n\t"
                  "outb %%al,$0x80\n\t"
                  "inb  $0x71,%%al\n\t" : : : "eax");
  }

private:
  static
  unsigned char reg_read(unsigned char reg)
  {
    Io::out8_p(reg, 0x70);
    return Io::in8_p(0x71);
  }

  static
  void reg_write(unsigned char reg, unsigned char val)
  {
    Io::out8_p(reg,0x70);
    Io::out8_p(val,0x71);
  }
};
