
#pragma once

#include <types.h>
#include <cxx/cxx_int>

struct Apic_id : cxx::int_type<Unsigned32, Apic_id>
{
  Apic_id() = default;
  Apic_id(Unsigned32 n) : cxx::int_type<Unsigned32, Apic_id>(n) {}
};


