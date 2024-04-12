#pragma once

template<typename T = void>
struct Hyp_irqs
{
  constexpr static int vgic = 25;
  constexpr static int vtimer = 27;
};

