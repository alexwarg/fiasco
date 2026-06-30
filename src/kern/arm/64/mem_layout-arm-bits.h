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
    User_max             = 0x000000ff'ffffffff,
  };
#elif defined(CONFIG_CPU_VIRT) && defined(CONFIG_ARM_PT48)
  enum Virt_layout_kern_user_max : Address {
    User_max             = 0x0000ffff'ffffffff,
  };
#endif

#ifdef CONFIG_CPU_VIRT
  enum Virt_layout_kern : Address {
    // These are guest physical addresses

    // The following are kernel virtual addresses. Mind that kernel and user
    // space live in different address spaces! Move to the top to minimize the
    // risk of colliding with physical memory which is still mapped 1:1.
    Registers_map_start  = 0x0000ffff'00000000,
    Registers_map_end    = 0x0000ffff'40000000,

    Map_base             = 0x0000ffff'40000000,

    Service_page         = 0x0000ffff'50000000,
    Tbuf_status_page     = Service_page + 0x5000,
    Tbuf_buffer_area	 = Service_page + 0x200000,
    Tbuf_buffer_size     = 0x200000,
    Jdb_tmp_map_area     = Service_page + 0x400000,

    Pmem_start           = 0x0000ffff'80000000,
    Pmem_end             = 0x0000ffff'c0000000,

    Cache_flush_area     = 0x0,
    Utcb_addr = 0, // dummy
  };

#else // !CONFIG_CPU_VIRT
  enum Virt_layout_kern : Address {
    User_max             = 0x0000ff7f'ffffffff,
    Utcb_addr            = User_max + 1 - 0x10000,
    Service_page         = 0xffff1000'eac00000,
    Tbuf_status_page     = Service_page + 0x5000,
    Tbuf_buffer_area     = Service_page + 0x200000,
    Tbuf_buffer_size     = 0x200000,
    Jdb_tmp_map_area     = Service_page + 0x400000,
    Registers_map_start  = 0xffff0000'00000000,
    Registers_map_end    = 0xffff0000'40000000,
    Cache_flush_area     = 0x0,
    Pmem_start           = 0xffff0000'40000000,
    Pmem_end             = 0xffff0000'80000000,

    Map_base             = 0xffff0000'80000000,

    Caps_start           = 0xff80'05000000,
    Caps_end             = 0xff80'0d000000,
  };
#endif

};
