
#include <dbg_stack.h>
#include <kmem_alloc.h>

DEFINE_PER_CPU Per_cpu<Dbg::Dbg_stack> Dbg::dbg_stack;

Dbg::Dbg_stack::Dbg_stack()
{
  stack_top = Kmem_alloc::allocator()->alloc(Bytes(Stack_size));
  if (stack_top)
    stack_top = (char *)stack_top + Stack_size;
}

