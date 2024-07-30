
#include <vm_vmx.h>

#include <kmem_slab.h>
#include <task_factory_impl.h>

void
Vm_vmx::operator delete (void *ptr)
{
  Vm_vmx *t = static_cast<Vm_vmx*>(ptr);
  Kmem_slab_t<Vm_vmx>::q_free(t->ram_quota(), ptr);
}

void
Vm_vmx_b::dump(void *v, unsigned f, unsigned t) const
{
  for (; f <= t; f += 2)
    printf("%04x: VMCS: %16lx   V: %16lx\n",
           f, Vmx::vmread<Mword>(f), read<Mword>(v, f));
}

void
Vm_vmx_b::dump_state(void *v) const
{
  dump(v, 0x0800, 0x080e);
  dump(v, 0x0c00, 0x0c0c);
  dump(v, 0x2000, 0x201a);
  dump(v, 0x2800, 0x2810);
  dump(v, 0x2c00, 0x2804);
  dump(v, 0x4000, 0x4022);
  dump(v, 0x4400, 0x4420);
  dump(v, 0x4800, 0x482a);
  dump(v, 0x6800, 0x6826);
  dump(v, 0x6c00, 0x6c16);
}


