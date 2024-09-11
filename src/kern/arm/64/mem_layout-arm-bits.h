#pragma once

#include <globalconfig.h>
#include "config.h"
#include "template_math.h"
#include "types.h"
#include <mem_layout-defaults.h>

class Mem_layout_arm_bits : public Mem_layout_defaults<Mem_layout_arm_bits>
{
public:
#if defined(CONFIG_CPU_VIRT) && !defined(CONFIG_ARM_PT48)
  enum Virt_layout_kern_user_max : Address {
    User_max             = 0x000000ffffffffff,
  };
#elif defined(CONFIG_CPU_VIRT) && defined(CONFIG_ARM_PT48)
  enum Virt_layout_kern_user_max : Address {
    User_max             = 0x0000ffffffffffff,
  };
#endif

#ifdef CONFIG_CPU_VIRT
  enum Virt_layout_kern : Address {
    // These are guest physical addresses

    // The following are kernel virtual addresses. Mind that kernel and user
    // space live in different address spaces! Move to the top to minimize the
    // risk of colliding with physical memory which is still mapped 1:1.
    Registers_map_start  = 0x0000ffff00000000,
    Registers_map_end    = 0x0000ffff40000000,

    Map_base             = 0x0000ffff40000000,

    Service_page         = 0x0000ffff50000000,
    Tbuf_status_page     = Service_page + 0x5000,
    Tbuf_buffer_area	 = Service_page + 0x200000,
    Tbuf_buffer_size     = 0x200000,
    Jdb_tmp_map_area     = Service_page + 0x400000,

    Pmem_start           = 0x0000ffff80000000,
    Pmem_end             = 0x0000ffffc0000000,

    Cache_flush_area     = 0x0,
    Utcb_addr = 0, // dummy
  };

#else // !CONFIG_CPU_VIRT
  enum Virt_layout_kern : Address {
    User_max             = 0x0000ff7fffffffff,
    Utcb_addr            = User_max + 1 - 0x10000,
    Service_page         = 0xffff1000eac00000,
    Tbuf_status_page     = Service_page + 0x5000,
    Tbuf_buffer_area     = Service_page + 0x200000,
    Tbuf_buffer_size     = 0x200000,
    Jdb_tmp_map_area     = Service_page + 0x400000,
    Registers_map_start  = 0xffff000000000000,
    Registers_map_end    = 0xffff000040000000,
    Cache_flush_area     = 0x0,
    Pmem_start           = 0xffff000040000000,
    Pmem_end             = 0xffff000080000000,

    Map_base             = 0xffff000080000000,

    Caps_start           = 0xff8005000000,
    Caps_end             = 0xff800d000000,
  };
#endif

};
