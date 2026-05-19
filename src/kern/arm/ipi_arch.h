#pragma once

#include <globalconfig.h>
#include <std_macros.h>

#if !defined (CONFIG_ARM_GIC) || defined (CONFIG_ARM_IPI_BSP)

#include <ipi_arm_bsp.h>

#else // CONFIG_ARM_GIC

#include <ipi_arm_gic.h>

template<typename T>
struct Ipi_arch : Ipi_arm_gic<T> {};

#endif // CONFIG_ARM_GIC
