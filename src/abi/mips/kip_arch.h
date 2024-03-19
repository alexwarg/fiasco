#pragma once

struct Kip_arch_platform_info
{
  char       name[16];
  Unsigned32 is_mp;
  Unsigned32 cpuid;
  Unsigned32 pad[2];
};

