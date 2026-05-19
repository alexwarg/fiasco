INTERFACE [arm && pf_lx2160]: //-------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_lx2160: Address {
    Gic_h_phys_base      = 0x0c0d0000,
    Gic_v_phys_base      = 0x0c0e0000,
  };
};
