#include <space.h>

Space::Ku_mem const *
Space::find_ku_mem(User_ptr<void> p, unsigned size)
{
  Address const pa = reinterpret_cast<Address>(p.get());

  // alignment check
  if (EXPECT_FALSE(pa & (sizeof(double) - 1)))
    return nullptr;

  // overflow check
  if (EXPECT_FALSE(pa + size - 1 < pa))
    return nullptr;

  for (Ku_mem_list::Const_iterator f = _ku_mem.begin(); f != _ku_mem.end(); ++f)
    {
      Address const a = reinterpret_cast<Address>(f->u_addr.get());
      if (a <= pa && (a + f->size - 1) >= (pa + size - 1))
        return *f;
    }

  return nullptr;
}


