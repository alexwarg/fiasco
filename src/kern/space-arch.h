#pragma once

// empty dummy arch hooks for Space

template<typename T>
struct Space_arch_mixin
{
  template<typename FLAGS>
  void switchin_context(T *, FLAGS) {}
};
