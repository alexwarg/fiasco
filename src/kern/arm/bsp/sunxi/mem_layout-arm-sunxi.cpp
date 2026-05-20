INTERFACE [arm && pf_sunxi]: //--------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_sunxi : Address {
    Mp_scu_phys_base     = 0xf8f00000,
    Timer_phys_base      = 0x01c20c00,
  };
};
