
extern "C" void sys_ipc_wrapper();
extern void (*syscall_table[])();

void (*syscall_table[])() =
{
  sys_ipc_wrapper,
};


