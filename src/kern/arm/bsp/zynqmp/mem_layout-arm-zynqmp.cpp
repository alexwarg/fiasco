INTERFACE [arm && pf_zynqmp]: //-------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_zynqmp: Address {
    Gic_h_phys_base      = 0xf9040000,
    Gic_v_phys_base      = 0xf9060000,
  };
};
