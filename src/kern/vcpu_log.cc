#include <vcpu_log.h>
#include <kobject_dbg.h>
#include <kobject.h>

void
Vcpu_log::print(String_buffer *buf) const
{
  switch (type)
    {
    case 0:
    case 4:
      buf->printf("%sret pc=%lx sp=%lx state=%lx task=D:%lx",
                  type == 4 ? "f" : "", ip, sp, state, space);
      break;
    case 1:
      buf->printf("ipc from D:%lx task=D:%lx sp=%lx",
                  Kobject_dbg::pointer_to_id(reinterpret_cast<Kobject*>(ip)), state, sp);
      break;
    case 2:
      buf->printf("exc #%x err=%lx pc=%lx sp=%lx state=%lx task=D:%lx",
                  unsigned{trap}, err, ip, sp, state, space);
      break;
    case 3:
      buf->printf("pf  pc=%lx pfa=%lx err=%lx state=%lx task=D:%lx",
                  ip, sp, err, state, space);
      break;
    default:
      buf->printf("vcpu: unknown");
      break;
    }
}

