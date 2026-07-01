
#include <mem_space_base.h>
#include <cassert>

DEFINE_PER_CPU
Per_cpu<Mem_space *> Mem_space_base::_current;
Mem_space *Mem_space_base::_kernel_space;

char const * const Mem_space_base::name = "Mem_space";
Mem_space_base::Page_order Mem_space_base::_glbl_page_sizes[Max_num_global_page_sizes];
unsigned Mem_space_base::_num_glbl_page_sizes;
bool Mem_space_base::_glbl_page_sizes_finished;

static Mem_space_base::Fit_size __mfs;

void
Mem_space_base::add_page_size(Page_order o)
{
  add_global_page_size(o);
  __mfs.add_page_size(o);
}

Mem_space_base::Fit_size const &
Mem_space_base::mem_space_fitting_sizes() const
{
  return __mfs;
}


void
Mem_space_base::add_global_page_size(Page_order o)
{
  assert (!_glbl_page_sizes_finished);
  unsigned i;
  for (i = 0; i < _num_glbl_page_sizes; ++i)
    {
      if (_glbl_page_sizes[i] == o)
        return;

      if (_glbl_page_sizes[i] < o)
        break;
    }

  assert (_num_glbl_page_sizes + 1 < Max_num_global_page_sizes);

  for (unsigned x = _num_glbl_page_sizes; x > i; --x)
    _glbl_page_sizes[x] = _glbl_page_sizes[x - 1];

  _glbl_page_sizes[i] = o;

  ++_num_glbl_page_sizes;
}


