#pragma once

#include <globalconfig.h>

#ifdef CONFIG_PF_REALVIEW

#include "platform_arm_realview.h"

namespace Jdb_kern_info_arch {
inline Unsigned64 get_time_now()
{ return Platform::sys->read<Mword>(Platform::Sys::Cnt_24mhz); }
}

#else

#include "kip.h"

namespace Jdb_kern_info_arch {
inline Unsigned64 get_time_now()
{ return Kip::k()->clock(); }
}

#endif

namespace Jdb_kern_info_arch {
#ifdef CONFIG_MP
static inline void stop_timer()
{
}
#endif
}

