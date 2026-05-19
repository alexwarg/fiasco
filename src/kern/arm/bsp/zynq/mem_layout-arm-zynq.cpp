INTERFACE [arm && pf_zynq]: //------------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_zynq : Address {
    Mp_scu_phys_base     = 0xf8f00000,
    L2cxx0_phys_base     = 0xf8f02000,
  };
};
