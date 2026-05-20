INTERFACE [arm && (pf_rpi_rpi1 || pf_rpi_rpizw)]: //-----------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_bcm2835 : Address {
    Bcm283x_base = 0x20000000,
  };
};

INTERFACE [arm && (pf_rpi_rpi2 || pf_rpi_rpi3)]: //------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_bcm2836_7 : Address {
    Bcm283x_base = 0x3f000000,
  };
};

//-------------------------------------------------------------------------
INTERFACE [arm && (pf_rpi_rpi1 || pf_rpi_rpizw || pf_rpi_rpi2 || pf_rpi_rpi3)]:

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_bcm283x : Address {
    Local_intc           = 0x40000000,
    Pic_phys_base        = Bcm283x_base + 0x0000b200,
    Timer_phys_base      = Bcm283x_base + 0x00003000,
    Watchdog_phys_base   = Bcm283x_base + 0x00100000,
  };
};

INTERFACE [arm && pf_rpi_rpi4]: //-----------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_bcm2711 : Address {
    Bcm2711_base       = 0xfe000000,
    Local_intc         = 0xff800000,
    Watchdog_phys_base = Bcm2711_base + 0x00100000,
  };
};
