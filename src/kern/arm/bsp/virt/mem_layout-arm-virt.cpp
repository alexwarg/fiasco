INTERFACE [arm && pf_arm_virt]: //---------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_virt : Address
  {
    Gic_h_phys_base      = 0x08030000,
    Gic_v_phys_base      = 0x08040000,
  };
};
