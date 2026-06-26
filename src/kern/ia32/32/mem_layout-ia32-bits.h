#pragma once

#include "types.h"
#include "config.h"
#include "linking.h"
#include "template_math.h"
#include <mem_layout-defaults.h>
#include <paging_bits.h>

#include <cassert>

class Mem_layout_ia32_bits : public Mem_layout_defaults<Mem_layout_ia32_bits>
{
public:
  enum Flags
  {
    Adap_in_kernel_image = FIASCO_IMAGE_PHYS_START < Config::SUPERPAGE_SIZE,
  };

  enum : Mword
  {
    Utcb_addr         = 0xbff00000,    ///< % 4 KiB   UTCB map address
    Kip_auto_map      = 0xbfff2000,    ///< % 4 KiB
    User_max          = 0xbfffffff,
    Service_page      = 0xeac00000,    ///< % 4 MiB   global mappings
    Local_apic_page   = Service_page + 0x0000,   ///< % 4 KiB
    Kmem_tmp_page_1   = Service_page + 0x2000,   ///< % 4 KiB size 8 KiB
    Kmem_tmp_page_2   = Service_page + 0x4000,   ///< % 4 KiB size 8 KiB
    Tbuf_status_page  = Service_page + 0x6000,   ///< % 4 KiB
    Tbuf_ustatus_page = Tbuf_status_page,
    Jdb_bts_area      = Service_page + 0xf000,   ///< % 4 KiB size 0x81000
    Idt               = Service_page + 0xfe000,  ///< % 4 KiB
    Syscalls          = Service_page + 0xff000,  ///< % 4 KiB syscall page
    Tbuf_buffer_area  = Service_page + 0x200000, ///< % 2 MiB
    Tbuf_buffer_size  = 0x200000,
    Tbuf_ubuffer_area = Tbuf_buffer_area,
    // 0xeb800000-0xec000000 (8 MiB) free
    Registers_map_start = 0xec000000,
    Registers_map_end   = 0xef800000,
    Kstatic           = 0xef800000,    ///< Io_bitmap - 4 MiB
    Io_bitmap         = 0xefc00000,    ///< % 4 MiB
    Vmem_end          = 0xf0000000,

    Kernel_image        = FIASCO_IMAGE_VIRT_START,
    Kernel_image_size   = FIASCO_IMAGE_VIRT_SIZE,
    Kernel_image_end    = Kernel_image + Kernel_image_size,

    Adap_image           = Adap_in_kernel_image
                           ? Kernel_image
                           : Kernel_image + Kernel_image_size,

    Adap_vram_mda_beg = Adap_image + 0xb0000, ///< % 8 KiB video RAM MDA memory
    Adap_vram_mda_end = Adap_image + 0xb8000,
    Adap_vram_cga_beg = Adap_image + 0xb8000, ///< % 8 KiB video RAM CGA memory
    Adap_vram_cga_end = Adap_image + 0xc0000,

    Caps_start        = 0xf0800000,    ///< % 4 MiB
    Caps_end          = 0xf3000000,    ///< % 4 MiB == Caps_start + (1<<20) * 4
    Physmem           = 0xfc400000,    ///< % 4 MiB   kernel memory
    Physmem_end       = 0x00000000,    ///< % 4 MiB   kernel memory
    Physmem_max_size  = Physmem_end - Physmem,
  };

  enum Offsets
  {
    Kernel_image_offset = FIASCO_IMAGE_PHYS_OFFSET,
  };

  enum Phys_addrs
  {
    Kernel_image_phys
      = Super_pg::trunc(FIASCO_U_CONST(FIASCO_IMAGE_PHYS_START)),
    Adap_image_phys = 0,
  };

  template<typename T> static T* boot_data(T const *addr);

  static Address pmem_size;
private:
  static Address physmem_offs asm ("PHYSMEM_OFFS");

public:
  static inline void kphys_base(Address base)
  { physmem_offs = (Address)Physmem - base; }

  static inline Address pmem_to_phys(Address addr)
  {
    assert(in_pmem(addr));
    return addr - physmem_offs;
  }

  static inline Address pmem_to_phys(const void *ptr)
  {
    Address addr = reinterpret_cast<Address>(ptr);
    assert(in_pmem(addr));
    return addr - physmem_offs;
  }

  static inline Address phys_to_pmem(Address addr)
  { return addr + physmem_offs; }

  static inline Mword in_kernel_image(Address addr)
  { return addr >= Kernel_image && addr < Kernel_image_end; }

  static inline Mword in_pmem(Address addr)
  { return addr >= Physmem; }
};
