#pragma once

#include <cxx/atomic>
#include <types.h>
#include <spin_lock.h>
#include <asid_alloc.h>
#include <mem_unit.h>

#include <globalconfig.h>

class Mem_space_arm_asid
{
public:
#if defined (CONFIG_ARM_LPAE) || defined (CONFIG_ARM_V6)
  static constexpr unsigned long Asid_base = 0;
#else
  // ASID 0 is reserved for synchronizing the update of ASID and translation
  // table base address, which is necessary when using the short-descriptor
  // translation table format, because with this format different registers
  // hold these two values, so an atomic update is not possible (see
  // "Synchronization of changes of ASID and TTBR" in ARM DDI 0487H.a).
  static constexpr unsigned long Asid_base = 1;
#endif
  using Asid_alloc = Asid_alloc_t<Unsigned64, Mem_unit::Asid_bits, Asid_base>;
  using Asid = Asid_alloc::Atomic_asid;
  using Asids = Asid_alloc::Asids_per_cpu;
  static constexpr bool Have_asids = true;

  unsigned long FIASCO_PURE c_asid() const
  {
    auto asid = _asid.load();

    if (EXPECT_TRUE(asid.is_valid()))
      return asid.asid();
    else
      return Mem_unit::Asid_invalid;
  }

  unsigned long asid()
  {
    if (_asid_alloc.get_or_alloc_asid(&_asid))
      {
        Mem_unit::tlb_flush();
        Mem::dsb();
      }

    return _asid.load().asid();
  };

private:
  /// active/reserved ASID (per CPU)
  static Per_cpu<Asids> _asids;
  static Asid_alloc _asid_alloc;

  /// current ASID of mem_space, provided by _asid_alloc
  Asid _asid;
};
