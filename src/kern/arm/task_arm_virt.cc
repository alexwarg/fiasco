
#include "task.h"
#include "task_factory_impl.h"

#include "vgic_global.h"
#include "config.h"
#include "l4_error.h"

static L4_msg_tag
map_gicc_page(Task *t, L4_msg_tag tag, Utcb *utcb)
{
  using Ko = Kobject_iface;

  if (tag.words() < 2)
    return Ko::commit_result(-L4_err::EInval);

  auto addr = Gic_h_global::gic->gic_v_address();
  if (!addr)
    return Ko::commit_result(-L4_err::ENosys);

  L4_fpage gicc_page(utcb->values[1]);
  if (   !gicc_page.is_valid()
      || !gicc_page.is_mempage()
      || gicc_page.order() < Config::PAGE_SHIFT)
    return Ko::commit_result(-L4_err::EInval);

  User<void>::Ptr u_addr((void *)gicc_page.mem_address());

  Mem_space *ms = static_cast<Mem_space *>(t);
  Mem_space::Status res =
    ms->v_insert(Mem_space::Phys_addr(addr),
                 Virt_addr((Address)u_addr.get()),
                 Mem_space::Page_order(Config::PAGE_SHIFT),
                 Mem_space::Attr::space_local(L4_fpage::Rights::URW()));

  switch (res)
    {
      case Mem_space::Insert_ok:
           break;
      case Mem_space::Insert_err_exists:
           return Ko::commit_result(-L4_err::EExists);
      case Mem_space::Insert_err_nomem:
           // FALLTHRU
      default:
           return Ko::commit_result(-L4_err::ENomem);
    };

  return Ko::commit_result(0);
}


bool
Task::invoke_arch(L4_msg_tag &tag, Utcb *utcb)
{
  if (utcb->values[0] == Vgicc_map_arm)
    {
      tag = map_gicc_page(this, tag, utcb);
      return true;
    }

  return false;
}

namespace {

static inline void
init_hyp_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_vm,
                             &Task::generic_factory<Task, false>);
}

STATIC_INITIALIZER(init_hyp_factory);

} // anon namespace

