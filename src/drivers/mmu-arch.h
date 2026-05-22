#pragma once

template< typename GEN, unsigned long Flush_area = 0, bool Ram = false >
class Mmu_arch : public GEN {};
