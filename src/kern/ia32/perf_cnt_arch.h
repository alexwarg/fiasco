#pragma once

#include <perf_cnt_defs.h>
#include <cpu.h>

namespace Perf_cnt
{
  class Perf_cnt_if
  {
  protected:
    struct Event
    {
      char  user;		// 1=count in user mode
      char  kern;		// 1=count in kernel mode
      char  edge;		// 1=count edge / 0=count duration
      Mword pmc;		// # of performance counter
      Mword bitmask;	// counter bitmask
      Mword evnt;		// event selector
    };

    bool _watchdog = false;
    signed char pmc_watchdog = -1;
    signed char pmc_loadcnt = -1;
    Mword _nr_regs = 0;
    Mword _sel_reg0;
    Mword _ctr_reg0;
    Signed64 hold_watchdog;
    Event pmc_event[Perf_cnt::Max_slot];  // index is slot number
    char pmc_alloc[Perf_cnt::Max_pmc];   // index is # of perfcounter

  public:
    Perf_cnt_if() = default;

    Perf_cnt_if(Mword sel_reg0, Mword ctr_reg0,
                Mword nr_regs, Mword watchdog)
      : _watchdog(watchdog), _nr_regs(nr_regs),
        _sel_reg0(sel_reg0), _ctr_reg0(ctr_reg0)
    {
      for (Mword slot = 0; slot < Max_slot; ++slot)
        {
          pmc_event[slot].pmc  = static_cast<Mword>(-1l);
          pmc_event[slot].edge = 0;
        }
    }

    // basic initialization
    virtual int init() { return 0; }

    // set event the counter should count
    virtual void set_pmc_event(Mword slot) { (void)slot; }

    virtual void init_watchdog() {} // no watchdog per default
    virtual void init_loadcnt(bool init_ap);

    // start watchdog (enable generation of overflow interrupt)
    virtual void start_watchdog() {} // no watchdog per default

    // stop watchdog (disable generation of overflow interrupt)
    virtual void stop_watchdog() {} // no watchdog per default

    virtual void start_pmc(Mword /*reg_nr*/) {}
    virtual void clear_pmc(Mword reg_nr);

    virtual ~Perf_cnt_if() = default;

    // watchdog supported by performance counter architecture?
    bool have_watchdog() const noexcept { return _watchdog; }
    bool watchdog_allocated() const noexcept { return pmc_watchdog >= 0; }
    bool loadcnt_allocated() const noexcept { return pmc_loadcnt >= 0; }
    void setup_loadcnt() noexcept
    {
      alloc_loadcnt();
      if (loadcnt_allocated())
        {
          init_loadcnt(false);
          start_pmc(pmc_loadcnt);
        }
    }

    void touch_watchdog()
    { Cpu::wrmsr(hold_watchdog, _ctr_reg0 + pmc_watchdog); }

    void alloc_loadcnt();
    void setup_watchdog(Mword timeout);
    void setup_pmc(Mword slot, Mword bitmask, Mword event,
                   Mword user, Mword kern, Mword edge);
    void mode(Mword slot, const char **mode, Mword *event,
              Mword *user, Mword *kern, Mword *edge);

  private:
    void alloc_watchdog();
    int alloc_pmc(Mword slot, Mword bitmask);

  };

  extern Perf_cnt_if *pcnt;

  // watchdog supported by performance counter architecture?
  inline int have_watchdog()
  { return (pcnt && pcnt->have_watchdog()); }

  // setup watchdog function with timeout in seconds
  inline void setup_watchdog(Mword timeout)
  {
    if (pcnt)
      pcnt->setup_watchdog(timeout);
  }

  inline void setup_loadcnt()
  {
    if (pcnt)
      pcnt->setup_loadcnt();
  }

  void start_watchdog();

  inline void stop_watchdog()
  {
    if (pcnt && pcnt->watchdog_allocated())
      pcnt->stop_watchdog();
  }

  inline void touch_watchdog()
  {
    if (pcnt && pcnt->watchdog_allocated())
      pcnt->touch_watchdog();
  }

  // return human-readable type of performance counters
  char const *perf_type();

  // set performance counter counting the selected event in slot #slot
  int setup_pmc(Mword slot, Mword event, Mword user, Mword kern, Mword edge);
  int mode(Mword slot, const char **mode, const char **name,
           Mword *event, Mword *user, Mword *kern, Mword *edge);
  Mword get_max_perf_event();
  void get_perf_event(Mword nr, unsigned *evntsel,
                      const char **name, const char **desc);
  Mword lookup_event(unsigned evntsel);

  void get_unit_mask(Mword nr, Unit_mask_type *type,
                     Mword *default_value, Mword *nvalues);
  void get_unit_mask_entry(Mword nr, Mword idx, 
                           Mword *value, const char **desc);
  /** Split event into event selector and unit mask (depending on perftype). */
  void split_event(Mword event, unsigned *evntsel, Mword *unit_mask);
  /** Combine event from selector and unit mask. */
  void combine_event(Mword evntsel, Mword unit_mask, Mword *event);

  void init_ap(Cpu const &cpu);

}

