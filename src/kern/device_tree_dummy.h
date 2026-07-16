#pragma once

// this are empty dummy types if no device tree is enabled

namespace Device_tree {
/**
 * Optional return value to control callback-driven loops.
 */
enum Cb
{
};

struct Array
{
  constexpr bool is_present() const
  { return false; }

  template<typename T>
  constexpr T get(unsigned) const
  { return 0; }

  constexpr unsigned len() const
  { return 0; }

  constexpr Array view(unsigned, unsigned) const
  { return Array(); }
};

template<unsigned N>
struct Array_prop
{
  constexpr bool is_present() const
  { return false; }

  constexpr bool is_valid() const
  { return false; }

  template<typename T = uint64_t>
  constexpr T get(unsigned, unsigned) const
  { return 0; }

  constexpr unsigned elements() const { return 0; }
};

class Node
{
public:
  constexpr Node() = default;
  constexpr Node(void const *, int) {}

  constexpr bool is_valid() const { return false; }
  constexpr explicit operator bool() const { return false; }
  constexpr bool is_root_node() const { return false; }

  constexpr void const *get_fdt() const { return nullptr; }
  constexpr int get_off() const { return -1; }

  constexpr Node parent_node() const { return Node(); }

  constexpr char const *get_name(char const *default_name = nullptr) const
  { return default_name; }

  constexpr bool has_prop(char const *) const { return false; }

  constexpr bool get_prop_u32(char const *, uint32_t *) const
  { return false; }

  constexpr bool get_prop_u64(char const *, uint64_t *) const
  { return false; }

  constexpr uint32_t get_prop_default_u32(char const *, uint32_t dflt) const
  { return dflt; }

  constexpr uint64_t get_prop_default_u64(char const *, uint64_t dflt) const
  { return dflt; }

  constexpr char const *get_prop_str(char const *) const
  { return nullptr; }

  constexpr Array get_array(char const *) const
  { return Array(); }

  template<unsigned N>
  constexpr Array_prop<N> get_prop_array(char const *, unsigned const (&)[N]) const
  { return Array_prop<N>(); }

  template<typename CB>
  constexpr void for_each_reg(CB &&) const
  {}

  constexpr bool get_reg(unsigned, uint64_t *, uint64_t * = nullptr) const
  { return false; }

  constexpr bool get_reg_untranslated(unsigned, uint64_t *, uint64_t * = nullptr) const
  { return false; }

  constexpr bool check_compatible(char const *) const
  { return false; }

  constexpr bool check_device_type(char const *) const
  { return false; }

  constexpr bool is_enabled() const
  { return false; }

  template<typename CB>
  constexpr void for_each_subnode(CB &&) const
  {}

  template<typename CB>
  constexpr void for_each_phandle(char const *, char const *, CB &&) const
  {}
};

class Dt
{
public:
  constexpr bool valid() const { return false; }

  constexpr Node node_by_path(char const *, int) const { return Node(); }
  constexpr Node node_by_path(char const *) const { return Node(); }
  constexpr Node node_by_phandle(uint32_t) const { return Node(); }
  constexpr Node node_by_compatible(char const *) const { return Node(); }

  template<typename CB>
  constexpr void nodes_by_compatible(char const *, CB &&) const {}

  template<typename ARR>
  constexpr Node node_by_compatible_list(ARR const &) const { return Node(); }

  template<typename CB>
  constexpr void nodes_by_prop_value(char const *, void const *, int, CB &&) const {}
};

extern Dt dt;

inline void init() {}

}
