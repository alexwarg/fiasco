#pragma once

#include <globalconfig.h>
#include "config.h"
#include "template_math.h"
#include "types.h"
#include <mem_layout-defaults.h>

class Mem_layout_arm_bits  : public Mem_layout_defaults<Mem_layout_arm_bits>
{
public:
#if !defined(CONFIG_KERN_START_0XD) && !defined(CONFIG_CPU_VIRT)
  enum Virt_layout_umax : Address {
    User_max             = 0xbfffffff,
  };
#elif defined(CONFIG_KERN_START_0XD)
  enum Virt_layout_umax : Address {
    User_max             = 0xcfffffff,
  };
#else // CONFIG_CPU_VIRT
  enum Virt_layout_umax : Address {
    User_max             = 0xffffffff,
  };
#endif

  enum Virt_layout : Address {
    Kern_lib_base        = 0xffffe000,
    Syscalls             = 0xfffff000,
    Utcb_addr            = User_max + 1 - 0x10000,
  };

#ifndef CONFIG_CPU_VIRT
  enum Virt_layout_kern : Address {
    Service_page         = 0xeac00000,
    Tbuf_status_page     = Service_page + 0x5000,
    Tbuf_ustatus_page    = Tbuf_status_page,
    Tbuf_buffer_area     = Service_page + 0x200000,
    Tbuf_buffer_size     = 0x200000,
    Tbuf_ubuffer_area    = Tbuf_buffer_area,
    Jdb_tmp_map_area     = Service_page + 0x400000,
    Registers_map_start  = 0xed000000,
    Registers_map_end    = 0xef000000,
    Cache_flush_area     = 0xef000000,
    Cache_flush_area_end = 0xef100000,
    Map_base             = 0xf0000000,
    Pmem_start           = 0xf0400000,
    Pmem_end             = 0xf5000000,

    Caps_start           = 0xf5000000,
    Caps_end             = 0xfd000000,
    Utcb_ptr_page        = 0xffffd000,
    utcb_ptr_align       = Tl_math::Ld<sizeof(void*)>::Res,
    Ivt_base             = 0xffff0000,
  };
#else // CONFIG_CPU_VIRT
  enum Virt_layout_kern : Address {
    Cache_flush_area     = 0x00000000,
    Service_page         = 0xeac00000,
    Tbuf_status_page     = Service_page + 0x5000,
    Tbuf_ustatus_page    = Tbuf_status_page,
    Tbuf_buffer_area     = Service_page + 0x200000,
    Tbuf_buffer_size     = 0x200000,
    Tbuf_ubuffer_area    = Tbuf_buffer_area,
    Jdb_tmp_map_area     = Service_page + 0x400000,
    Registers_map_start  = 0xed000000,
    Registers_map_end    = 0xef000000,
    Pmem_start           = 0xf0000000,
    Pmem_end             = 0xf8000000,
    Map_base             = RAM_PHYS_BASE,
    Ivt_base             = 0xffff0000,
  };
#endif
};
