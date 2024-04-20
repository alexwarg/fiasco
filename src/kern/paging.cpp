INTERFACE:

#include <types.h>

#include <paging-page.h>
#include <paging-pf.h>
#include <paging-pdir.h>

template<typename ALLOC>
class Pdir_alloc_simple
{
public:
  Pdir_alloc_simple(ALLOC *a = 0) : _a(a) {}

  void *alloc(Bytes size) const
  { return _a->alloc(size); }

  void free(void *block, Bytes size) const
  { _a->free(size, block); }

  bool valid() const { return _a; }

  Phys_mem_addr::Value
  to_phys(void *v) const { return _a->to_phys(v); }

private:
  ALLOC *_a;
};

IMPLEMENTATION:
//---------------------------------------------------------------------------

template<typename ALLOC>
inline Pdir_alloc_simple<ALLOC> pdir_alloc(ALLOC *a)
{ return Pdir_alloc_simple<ALLOC>(a); }

