#pragma once

#include <types.h>
#include <globalconfig.h>

#ifdef CONFIG_JDB
// need atomic counters for debug counters
#include <cxx/atomic>

template<typename IPI>
class Ipi_dbg
{
public:
  static void stat_sent(Cpu_number from_cpu)
  { IPI::_ipi.cpu(from_cpu)._stat_sent.fetch_add(1); }

  static void stat_received(Cpu_number on_cpu)
  { IPI::_ipi.cpu(on_cpu)._stat_received++; }

private:
  friend class Jdb_ipi_module;
  cxx::atomic<Mword> _stat_sent;
  Mword _stat_received;
};

#else // CONFIG_JDB

template<typename IPI>
class Ipi_dbg
{
public:
  // debug interface dummy
  static void stat_sent(Cpu_number from_cpu)
  { (void)from_cpu; }

  static void stat_received(Cpu_number on_cpu)
  { (void)on_cpu; }
};

#endif // CONFIG_JDB

#ifdef CONFIG_MP

#include <per_cpu_data.h>
#include <ipi_arch.h>

class Ipi : public Ipi_arch<Ipi>, public Ipi_dbg<Ipi>
{
private:
  friend class Jdb_ipi_module;
  friend class Ipi_dbg<Ipi>;
  friend class Ipi_arch<Ipi>;
  static Per_cpu<Ipi> _ipi;
};

#else // CONFIG_MP

class Ipi
{
public:
  enum Message { Request, Global_request, Debug, Timer };

  static void init(Cpu_number cpu)
  { (void)cpu; }

  static void send(Message, Cpu_number from_cpu, Cpu_number to_cpu)
  { (void)from_cpu; (void)to_cpu; }

  static void eoi(Message, Cpu_number on_cpu)
  { (void)on_cpu; }

  static void bcast(Message, Cpu_number from_cpu)
  { (void)from_cpu; }

  // debug interface dummy
  static void stat_sent(Cpu_number from_cpu)
  { (void)from_cpu; }

  static void stat_received(Cpu_number on_cpu)
  { (void)on_cpu; }
};
#endif // CONFIG_MP

