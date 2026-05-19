INTERFACE [arm && pf_ls1046]: //-------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_ls1046: Address {
    Gic_h_phys_base      = 0x01440000,
    Gic_v_phys_base      = 0x01460000,
  };
};
