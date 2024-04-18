#include <map_util.h>
#include <space.h>
#include <map_util_io.h>
#include <map_util_objs.h>
#include <map_util_mem.h>
#include <kobject.h>

#include <cassert>


/**
 * Flexpage mapping.
 *
 * \param from     Source address space
 * \param fp_from  Flexpage descriptor for virtual-address space range in source
 *                 address space
 * \param to       Destination address space
 * \param fp_to    Flexpage descriptor for virtual-address space range in
 *                 destination address space
 * \param control  Message item describing the mapping operation.
 * \param r        List of Kobjects that may be deleted during that operation.

 * \return IPC error
 *
 * This function diverts to mem_map (for memory fpages), io_map (for IO fpages)
 * or obj_map (for capability fpages).
*/
// Don't inline -- it eats too much stack.
// inline NEEDS ["config.h", io_map]
L4_error
fpage_map(Space *from, L4_fpage fp_from, Space *to,
          L4_fpage fp_to, L4_msg_item control, Kobject::Reap_list *r)
{
  if (EXPECT_FALSE(!fp_from.same_type(fp_to))
      && EXPECT_FALSE(!fp_to.is_all_spaces()))
    return  L4_error::None;

  Space::Caps caps = from->caps() & to->caps();

  if (fp_from.is_mempage() && (caps & Space::Caps::mem()))
    return mem_map(from, fp_from, to, fp_to, control);

#ifdef CONFIG_PF_PC
  if (fp_from.is_iopage() && (caps & Space::Caps::io()))
    return io_map(from, fp_from, to, fp_to, control);
#endif

  if (fp_from.is_objpage() && (caps & Space::Caps::obj()))
    return obj_map(from, fp_from, to, fp_to, control, r->list());

  return L4_error::None;
}

/** Flexpage unmapping.
    divert to mem_fpage_unmap (for memory fpages) or
    io_fpage_unmap (for IO fpages)
    @param space address space that should be flushed
    @param fp    flexpage descriptor of address-space range that should
                 be flushed
    @param me_too If false, only flush recursive mappings.  If true,
                 additionally flush the region in the given address space.
    @param flush_mode determines which access privileges to remove.
    @return combined (bit-ORed) access status of unmapped physical pages
*/
// Don't inline -- it eats too much stack.
// inline NEEDS ["config.h", io_fpage_unmap]
L4_fpage::Rights
fpage_unmap(Space *space, L4_fpage fp, L4_map_mask mask, Kobject ***rl)
{
  L4_fpage::Rights ret(0);
  Space::Caps caps = space->caps();

  if ((caps & Space::Caps::io()) && (fp.is_iopage() || fp.is_all_spaces()))
    ret |= io_fpage_unmap(space, fp, mask);

  if ((caps & Space::Caps::obj()) && (fp.is_objpage() || fp.is_all_spaces()))
    ret |= obj_fpage_unmap(space, fp, mask, rl);

  if ((caps & Space::Caps::mem()) && (fp.is_mempage() || fp.is_all_spaces()))
    ret |= mem_fpage_unmap(space, fp, mask);

  return ret;
}


