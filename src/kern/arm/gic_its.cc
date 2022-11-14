
#include <gic_its.h>

#include <cpu.h>
#include <kmem_alloc.h>
#include <panic.h>
#include <poll_timeout_counter.h>
#include <cstdio>
#include <string.h>
#include <mmio_register_block.h>
#include <processor.h>

/**
 * Disable the ITS to prevent triggering of unexpected LPIs.
 */
void
Gic_its::disable(Address base)
{
  auto its = Mmio_register_block(base);

  unsigned arch_rev = (its.read<Unsigned32>(GITS_PIDR2) >> 4) & 0xf;
  if (arch_rev != 0x3 && arch_rev != 0x4)
    panic("ITS: Version %u is not supported.\n", arch_rev);

  Ctlr ctlr(its.read<Unsigned32>(GITS_CTLR));
  if (!ctlr.enabled() && ctlr.quiescent())
    return;

  ctlr.umsi_irq() = false;
  ctlr.enabled() = false;
  its.write<Unsigned32>(ctlr.raw, GITS_CTLR);

  // Wait for quiescent state
  L4::Poll_timeout_counter i(5000000);
  while (i.test(!Ctlr(its.read<Unsigned32>(GITS_CTLR)).quiescent()))
    Proc::pause();
  if (Ctlr(its.read<Unsigned32>(GITS_CTLR)).quiescent())
    printf("ITS: Disabled.\n");
  else
    panic("ITS: Trying to disable: Not in quiescent state!\n");
}


#ifdef CONFIG_ARM_GIC_MSI

Gic_its::Device_alloc Gic_its::device_alloc;

void
Gic_its::Table::alloc(Reg r, Typer typer)
{
  Baser baser(r.read());

  _type = static_cast<Baser::Type>(baser.type());
  _entry_size = baser.entry_size() + 1;
  Unsigned64 size;
  switch (_type)
    {
    case Baser::Type_device:
      // One entry for each DeviceID.
      size = static_cast<Unsigned64>(_entry_size) << (typer.dev_bits() + 1);
      // If required memory size is above the threshold use a lazily populated
      // two-level device table to avoid wasting a lot of memory.
      _indirect = size > Max_direct_size;
      break;

    case Baser::Type_collection:
      if (Num_cols <= typer.hcc())
        {
          // ITS can store a sufficient amount of collection table entries in
          // internal memory, no need to allocate an external collection table.
          if ((int)Config::Warn_level >= Info)
            printf("ITS: Using internal memory to store collections.\n");
          return;
        }

      // One entry for each redistributor.
      size = Num_cols * _entry_size;
      _indirect = false;
      break;

    case Baser::Type_vpe:
      // TODO: Direct VM LPI injection not yet supported...
      return;

    case Baser::Type_none:
      return;

    default:
      WARN("ITS: Skip allocation of table with unknown type=%u.\n", _type);
      return;
    }

  // Detect supported page size. We try to stick with 4k pages to keep the
  // alignment overhead low. But if the HW forces us to use larger pages so be
  // it...
  baser.page_size() = Baser::Page_size_4k;
  r.write(baser.raw);
  baser.raw = r.read();
  switch (baser.page_size())
    {
    case Baser::Page_size_4k:   _page_size =  0x1000; break;
    case Baser::Page_size_16k:  _page_size =  0x4000; break;
    default:                    _page_size = 0x10000; break;
    }

  if (_indirect)
    {
      unsigned entries = size / _entry_size;
      unsigned entries_per_page = _page_size / _entry_size;
      unsigned l2_pages = cxx::div_ceil(entries, entries_per_page);
      unsigned l1_pages = cxx::div_ceil(l2_pages, _page_size / L1_entry::Size);
      size = l1_pages * _page_size;
    }
  else
    {
      size = cxx::ceil_lsb(size, cxx::log2u(_page_size));
    }

  _mem = Gic_mem::alloc_zmem(size, _page_size);
  if (!_mem.is_valid())
    panic("ITS: Failed to allocate table of type=%u and size=0x%llx.\n",
          _type, size);

  unsigned num_pages = size / _page_size;
  assert(num_pages <= Baser::Size_max);
  baser.size() = num_pages - 1;
  // The physical bits 51:48 of the physical address must be zero!
  assert((_mem.phys_addr() & (0xfULL << 48)) == 0UL);
  baser.pa() = _mem.phys_addr();
  baser.indirect() = _indirect;
  baser.valid() = 1;
  _mem.setup_reg(r, baser);
  _mem.make_coherent();

  if ((int)Config::Warn_level >= Info)
    printf("ITS: Allocated table of type=%u with size=0x%llx "
           "indirect=%u page_size=%u entry_size=%u pages=%u.\n",
           _type, size, _indirect, _page_size, _entry_size, num_pages);
}

bool
Gic_its::Table::ensure_id_present(unsigned id)
{
  // Only two-level tables are lazily allocated.
  if (!_indirect)
    return true;

  unsigned l1_index = id / (_page_size / _entry_size);
  L1_entry *l1_table = _mem.virt_ptr<L1_entry>();
  L1_entry e = l1_table[l1_index];
  if (e.valid())
    // Second level table is already allocated.
    return true;

  Gic_mem l2_table = Gic_mem::alloc_zmem(_page_size, _page_size);
  if (l2_table.is_valid())
    {
      e.pa() = l2_table.phys_addr();
      e.valid() = 1;
      l2_table.inherit_mem_attribs(_mem);
      l2_table.make_coherent();

      L1_entry *ep = l1_table + l1_index;
      write_now(ep, e);
      _mem.make_coherent(ep, ep + 1);

      return true;
    }
  else
    {
      WARNX(Error, "ITS: Failed to allocate second-level table of type=%u.\n",
            _type);
      return false;
    }
}

void
Gic_its::init(Gic_cpu_v3 *gic_cpu, Address base, unsigned num_lpis)
{
  _its = Mmio_register_block(base);
  unsigned arch_rev = (_its.read<Unsigned32>(GITS_PIDR2) >> 4) & 0xf;
  if (arch_rev != 0x3 && arch_rev != 0x4)
    // No GICv3 and no GICv4
    panic("ITS: Version %u is not supported.\n", arch_rev);

  // Disable ITS before trying to enable it to get an already enabled ITS into
  // quiescent state (see also the possible panic below).
  disable(base);

  _gic_cpu = gic_cpu;
  _cmd_queue_lock.init();
  _device_alloc_lock.init();

  Typer typer(_its.read<Unsigned64>(GITS_TYPER));
  _redist_pta = typer.pta();
  _max_device_id = (1ULL << (typer.dev_bits() + 1)) - 1;
  _itt_entry_size = typer.itt_entry_size() + 1;

  Unsigned64 num_events = 1ULL << (typer.id_bits() + 1);
  if (num_lpis > num_events)
  {
    // TODO: Use per-device EventID space instead of global EventID space?
    WARN("ITS: Number of LPIs %u exceeds number of supported EventIDs %llu.\n",
         _num_lpis, num_events);
  }

  _num_lpis = num_lpis;

  Ctlr ctlr(_its.read<Unsigned32>(GITS_CTLR));
  if (ctlr.enabled() || !ctlr.quiescent())
    panic("ITS: Not in quiescent state.\n");

  init_tables(typer);
  init_cmd_queue();

  // Enable ITS
  ctlr.umsi_irq() = false;
  ctlr.enabled() = true;
  _its.write<Unsigned32>(ctlr.raw, GITS_CTLR);

  printf("ITS: %lx rev=%x num_lpis=%u num_cols=%u num_devs=%u dev_bits=%u\n",
         base, arch_rev, _num_lpis, Num_cols, Max_num_devs,
         cxx::log2u(_max_device_id + 1));
}

void
Gic_its::init_tables(Typer typer)
{
  for (unsigned i = 0; i < GITS_baser_num; i++)
    {
      unsigned off = GITS_BASER + (i * 8);
      _tables[i].alloc(_its.r<Unsigned64>(off), typer);

      if (_tables[i].type() == Baser::Type_device)
        _device_table = &_tables[i];
    }

    if (!_device_table)
      panic("ITS: No device table detected.\n");
}

void
Gic_its::init_cmd_queue()
{
  _cmd_queue =  Gic_mem::alloc_zmem(GITS_cmd_queue_size, GITS_cmd_queue_align);
  if (!_cmd_queue.is_valid())
    panic("ITS: Failed to allocate command queue.\n");

  Cbaser cbaser;
  cbaser.size() = (GITS_cmd_queue_size / GITS_cmd_queue_page_size) - 1;
  cbaser.pa() = _cmd_queue.phys_addr();
  cbaser.valid() = 1;
  _cmd_queue.setup_reg(_its.r<Unsigned64>(GITS_CBASER), cbaser);
  _cmd_queue.make_coherent();
  // GITS_CREADR is cleared to 0 when GITS_CBASER is written.
  _cmd_queue_write_off = 0;
  _its.write<Unsigned64>(_cmd_queue_write_off, GITS_CWRITER);
}

void
Gic_its::cpu_init(Cpu_number cpu, Gic_redist const &redist)
{
  unsigned cpu_index = cxx::int_value<Cpu_number>(cpu);

  Collection tmp;
  if (_redist_pta)
    tmp.redist_base.phys_base_addr() = Gic_mem::to_phys(redist.get_base());
  else
    tmp.redist_base.processor_nr() = redist.get_processor_nr();
  tmp.icid = cpu_index;
  send_cmd(Cmd::mapc(tmp.icid, tmp.redist_base, true), &tmp);
  send_cmd(Cmd::invall(tmp.icid), &tmp);

  Collection &col = _cols[cpu_index];
  col.redist_base = tmp.redist_base;
  Mem::mp_wmb();
  // Set the ICID last, as it marks the collection as valid. The collection must
  // be fully setup before it may appear valid on other CPUs.
  col.icid = tmp.icid;
}


/**
 * Send a command to the ITS.
 *
 * \param cmd  The command to send.
 * \param col  The collection to sync the command with.
 */
void
Gic_its::send_cmd(Cmd const &cmd, Collection const *col)
{
  auto guard = lock_guard(_cmd_queue_lock);

  unsigned num_cmds = 1;
  if (EXPECT_FALSE(!enqueue_cmd(cmd)))
    return;

  if (col)
    {
      assert(col->is_valid());

      if (EXPECT_TRUE(enqueue_cmd(Cmd::sync(col->redist_base))))
        num_cmds++;
    }
  // Inform ITS about the submitted commands.
  _its.write<Mword>(_cmd_queue_write_off, GITS_CWRITER);

  unsigned wait_off = _cmd_queue_write_off;

  // We are done modifying the command queue, thus release the lock.
  guard.reset();

  // Wait for the submitted commands to complete.
  L4::Poll_timeout_counter i(5000000);
  while (i.test(!is_cmd_complete(wait_off, num_cmds)))
    Proc::pause();

  if (EXPECT_FALSE(i.timed_out()))
    WARNX(Error, "ITS: Command execution timed out!\n");
}

/**
 * \pre The _device_alloc_lock must be held.
 */
Gic_its::Device *
Gic_its::get_or_alloc_device(Dev_id dev_id)
{
  for (auto &&dev : _devices)
    {
      if (dev->id() == dev_id)
        return dev;
    }

  if (_num_devs >= Max_num_devs)
    {
      WARN("ITS: Failed to allocate device %x, max device limit reached!\n",
           dev_id);
      return nullptr;
    }

  Device *device = device_alloc.new_obj(dev_id, *this);
  if (!device)
    {
      WARN("ITS: Failed to allocate device %x!\n", dev_id);
      return nullptr;
    }

  if (!device->setup_itt())
    {
      WARN("ITS: Failed to setup ITT for device %x!\n", dev_id);
      device_alloc.del(device);
      return nullptr;
    }

  _devices.add(device);
  _num_devs++;
  return device;
}

/**
 * \pre The _device_alloc_lock must be held.
 * \pre The lpi.lock must be held.
 */
void
Gic_its::unbind_lpi_from_device(Lpi &lpi)
{
  if (Device *device = lpi.device)
    {
      device->unbind_lpi(lpi);

      if (!device->has_lpis())
        {
          // Device is unused now, delete it.
          device_alloc.del(device);
          _num_devs--;
        }
    }
}

/**
 * \pre The lpi.lock must be held.
 */
int
Gic_its::bind_lpi_to_device(Lpi &lpi, Unsigned32 src, Irq_mgr::Msi_info *inf)
{
  if (src > _max_device_id)
    {
      WARN("ITS: 0x%x is not a valid DeviceID!\n", src);
      return -L4_err::ERange;
    }

  if (!lpi.device || lpi.device->id() != src)
    {
      auto guard = lock_guard(_device_alloc_lock);

      unbind_lpi_from_device(lpi);

      auto device = get_or_alloc_device(src);
      if (!device)
        return -L4_err::ENomem;

      device->bind_lpi(lpi);
    }

  inf->data = lpi.event_id();
  // TODO: Must be mapped in the DMA space of the device if IOMMU is enabled.
  inf->addr = Gic_mem::to_phys(_its.get_mmio_base()) + GITS_TRANSLATER;
  return 0;
}

/**
 * \pre The lpi.lock must be held.
 */
void
Gic_its::free_lpi(Lpi &lpi)
{
  auto guard = lock_guard(_device_alloc_lock);
  unbind_lpi_from_device(lpi);
  lpi.reset();
}

void
Gic_its::assign_lpi_to_cpu(Lpi &lpi, Cpu_number cpu)
{
  Collection const *col = get_col(cpu);
  if (EXPECT_FALSE(!col->is_valid()))
    {
      WARN("ITS: Tried to assign LPI %u to uninitialized CPU %u.\n",
           lpi.intid(), cxx::int_value<Cpu_number>(cpu));
      return;
    }

  if (lpi.col != col)
    {
      if (lpi.device && lpi.col)
        {
          send_cmd(Cmd::movi(lpi.device->id(), lpi.event_id(), col->icid), col);
        }
      else if (lpi.device)
        {
          send_cmd(Cmd::mapti(lpi.device->id(), lpi.event_id(), lpi.intid(),
                              col->icid), col);
        }

      lpi.col = col;
    }
}

/**
 * \pre The _device_alloc_lock must be held.
 */
bool
Gic_its::Device::setup_itt()
{
  assert(!_itt.is_valid());

  _itt = Gic_mem::alloc_zmem(itt_size(), 256);
  if (!_itt.is_valid())
    return false;

  // Allocate second-level device table if necessary.
  if (!_its._device_table->ensure_id_present(_id))
    {
      _itt.free();
      return false;
    }
  _itt.inherit_mem_attribs(_its._device_table->mem());

  _itt.make_coherent();
  _its.send_cmd(Cmd::mapd(_id, _itt.phys_addr(), cxx::log2u(_its.num_lpis())));
  return true;
}

/**
 * \pre The _device_alloc_lock must be held.
 */
void
Gic_its::Device::free_itt()
{
  assert(_lpi_count == 0);
  assert(_itt.is_valid());

  _its.send_cmd(Cmd::mapd(_id, 0, 0));
  _itt.free();
}

unsigned
Gic_its::Device::itt_size()
{
  return _its.num_lpis() * _its._itt_entry_size;
}

/**
 * \pre The _device_alloc_lock must be held.
 * \pre The lpi.lock must be held.
 */
void
Gic_its::Device::bind_lpi(Lpi &lpi)
{
  assert(lpi.device == nullptr);

  lpi.device = this;
  _lpi_count++;

  if (lpi.col)
    _its.send_cmd(Cmd::mapti(_id, lpi.event_id(), lpi.intid(), lpi.col->icid),
                  lpi.col);
}

/**
 * \pre The _device_alloc_lock must be held.
 * \pre The lpi.lock must be held.
 */
void
Gic_its::Device::unbind_lpi(Lpi &lpi)
{
  assert(lpi.device == this);

  if (lpi.col)
    _its.send_cmd(Cmd::discard(_id, lpi.event_id()), lpi.col);

  lpi.device = nullptr;
  _lpi_count--;
}
#endif
