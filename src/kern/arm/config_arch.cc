
#include <config.h>
#include <feature.h>

char const *const kernel_warn_config_string = nullptr;

#if defined (CONFIG_ARM_V6PLUS)
KIP_KERNEL_FEATURE("armv6plus");
#endif
