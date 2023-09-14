#pragma once

#include <mem_layout.h>
#include <types.h>
#include <globalconfig.h>
#include <mem_space_base.h>
#include <kmem.h>
#include <kmem_alloc.h>
#include <ram_quota.h>
#include <panic.h>

template<typename M>
class Mem_space_arm_bits
{
private:
  M *_ths() { return static_cast<M *>(this); }
  M const *_ths() const { return static_cast<M const *>(this); }
#ifdef CONFIG_CPU_VIRT
  static Address __mem_space_syscall_page;
#endif

protected:
  int sync_kernel();

public:
  static void set_syscall_page(void *p);
  Address pmem_to_phys(Address virt) const;
  void make_current(Mem_space_base::Switchin_flags flags = Mem_space_base::None);

  static Address user_max()
  { return Mem_layout::User_max; }
};

#ifdef CONFIG_CPU_VIRT

template<typename M>
Address Mem_space_arm_bits<M>::__mem_space_syscall_page;

template<typename M>
void
Mem_space_arm_bits<M>::set_syscall_page(void *p)
{
  __mem_space_syscall_page = (Address)p;
}

template<typename M>
int
Mem_space_arm_bits<M>::sync_kernel()
{
  auto pte = _ths()->_dir->walk(Virt_addr(Mem_layout::Kern_lib_base),
      Pdir::Depth, true, Kmem_alloc::q_allocator(_ths()->ram_quota()));
  if (pte.level < Pdir::Depth - 1)
    return -1;

  extern char kern_lib_start[];

  Phys_mem_addr pa((Address)kern_lib_start - Mem_layout::Map_base
                   + Mem_layout::Sdram_phys_base);
  pte.set_page(pa, Page::Attr::space_local(Page::Rights::URX()));

  pte.write_back_if(true, _ths()->c_asid());

  pte = _ths()->_dir->walk(Virt_addr(Mem_layout::Syscalls),
      Pdir::Depth, true, Kmem_alloc::q_allocator(_ths()->ram_quota()));

  if (pte.level < Pdir::Depth - 1)
    return -1;

  pa = Phys_mem_addr(__mem_space_syscall_page);
  pte.set_page(pa, Page::Attr::space_local(Page::Rights::URX()));

  pte.write_back_if(true, _ths()->c_asid());

  return 0;
}

template<typename M>
inline Address
Mem_space_arm_bits<M>::pmem_to_phys(Address virt) const
{
  return Kmem::kdir->virt_to_phys(virt);
}

template<typename M>
inline void
Mem_space_arm_bits<M>::make_current(Mem_space_base::Switchin_flags)
{
  // FIXME: flush bt only when reassigning ASIDs not on switch !!!!!!!
  asm volatile (
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "isb                          \n"
      "mcrr p15, 6, %0, %1, c2      \n" // set VTTBR
      "isb                          \n"
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "isb                          \n"
      :
      : "r" (cxx::int_value<Phys_mem_addr>(_ths()->_dir_phys)), "r"(_ths()->asid() << 16), "r" (0)
      : "r1");

  _ths()->_current.current() = _ths();
}

#else // CONFIG_CPU_VIRT

#include <kmem_space.h>

template<typename M>
void
Mem_space_arm_bits<M>::set_syscall_page(void *p)
{
  auto pte = Kmem::kdir->walk(Virt_addr(Kmem_space::Syscalls),
                              Pdir::Depth, true,
                              Kmem_alloc::q_allocator(Ram_quota::root));

  if (pte.level == 0) // allocation of second level faild
    panic("FATAL: Error mapping syscall page to %p\n",
          (void *)Kmem_space::Syscalls);

  pte.set_page(Phys_mem_addr(Kmem::kdir->virt_to_phys((Address)p)),
               Page::Attr::kern_global(Page::Rights::URX()));
  pte.write_back_if(true, Mem_unit::Asid_kernel);
}

template<typename M>
int
Mem_space_arm_bits<M>::sync_kernel()
{
  return _ths()->_dir
    ->sync(Virt_addr(Mem_layout::User_max + 1),
           static_cast<M *>(Mem_space_base::kernel_space())->_dir,
           Virt_addr(Mem_layout::User_max + 1),
           Virt_size(-(Mem_layout::User_max + 1)), Pdir::Super_level,
           Pte_ptr::need_cache_write_back(_ths() == _ths()->_current.current()),
           Kmem_alloc::q_allocator(_ths()->ram_quota()));
}

template<typename M>
inline Address
Mem_space_arm_bits<M>::pmem_to_phys(Address virt) const
{
  return _ths()->virt_to_phys(virt);
}

#ifdef CONFIG_ARM_V6
template<typename M>
inline void
Mem_space_arm_bits<M>::make_current(Mem_space_base::Switchin_flags)
{
  asm volatile (
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "mcr p15, 0, r0, c7, c10, 4   \n" // dsb
      "mcr p15, 0, %0, c2, c0       \n" // set TTBR0
      "mcr p15, 0, r0, c7, c10, 4   \n" // dsb
      "mcr p15, 0, %1, c13, c0, 1   \n" // set new ASID value
      "mcr p15, 0, r0, c7, c5, 4    \n" // isb
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "mrc p15, 0, r1, c2, c0       \n"
      "mov r1, r1                   \n"
      "sub pc, pc, #4               \n"
      :
      : "r" (cxx::int_value<Phys_mem_addr>(_ths()->_dir_phys) | Page::Ttbr_bits),
        "r" (_ths()->asid()), "r" (0)
      : "r1");
  _ths()->_current.current() = _ths();
}
#endif // CONFIG_ARM_V6

#if defined (CONFIG_ARM_V7) || defined (CONFIG_ARM_V8)
#ifdef CONFIG_ARM_LPAE

template<typename M>
inline void
Mem_space_arm_bits<M>::make_current(Mem_space_base::Switchin_flags)
{
  asm volatile (
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "isb                          \n"
      "mcrr p15, 0, %0, %1, c2      \n" // set TTBR0
      "isb                          \n"
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "isb                          \n"
      :
      : "r" (cxx::int_value<Phys_mem_addr>(_ths()->_dir_phys)),
        "r" (_ths()->asid() << 16), "r" (0)
      : "r1");
  _ths()->_current.current() = _ths();
}

#else // CONFIG_ARM_LPAE

template<typename M>
inline void
Mem_space_arm_bits<M>::make_current(Mem_space_base::Switchin_flags)
{
  asm volatile (
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "dsb                          \n"
      "mcr p15, 0, %2, c13, c0, 1   \n" // change ASID to 0
      "isb                          \n"
      "mcr p15, 0, %0, c2, c0       \n" // set TTBR0
      "isb                          \n"
      "mcr p15, 0, %1, c13, c0, 1   \n" // set new ASID value
      "isb                          \n"
      "mcr p15, 0, %2, c7, c5, 6    \n" // bt flush
      "isb                          \n"
      :
      : "r" (cxx::int_value<Phys_mem_addr>(_ths()->_dir_phys) | Page::Ttbr_bits),
        "r" (_ths()->asid()), "r" (0)
      : "r1");
  _ths()->_current.current() = _ths();
}

#endif // CONFIG_ARM_LPAE
#endif // CONFIG_ARM_V7 || CONFIG_ARM_V8

#endif // CONFIG_CPU_VIRT
