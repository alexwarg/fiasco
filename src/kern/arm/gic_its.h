#pragma once

#include <boot_alloc.h>
#include <cpu.h>
#include <gic_cpu_v3.h>
#include <gic_dist.h>
#include <gic_mem.h>
#include <gic_redist.h>
#include <irq_mgr.h>
#include <kmem_slab.h>
#include <mmio_register_block.h>
#include <spin_lock.h>

#include <arithmetic.h>
#include <cxx/bitfield>
#include <cxx/hlist>
#include <cxx/static_vector>

/**
 * The GICv3 architecture provides support for message-based interrupts, e.g.
 * Message Signaled Interrupts (MSI), with the help of the Interrupt
 * Translation Service (ITS) component.
 *
 * The kernel configures the ITS to map the combination of a DeviceID and an
 * EventID to a Locality-specific Peripheral Interrupt (LPI) directed to a
 * redistributor (i.e. to a CPU). To trigger an LPI, a device has to write the
 * corresponding EventID to the GITS_TRANSLATER register of the ITS. The
 * DeviceID for a write, which is unique for each device, is presented to the
 * ITS in a way that cannot be spoofed. Thus, the ITS can ensure that a device
 * can only trigger the LPIs that have been mapped for it.
 *
 * For the translation of MSIs (devices writing EventIDs to GITS_TRANSLATER)
 * into LPIs, the ITS uses memory tables provided by the kernel. However, the
 * kernel does not directly write to these tables, but instead configures the
 * ITS through commands issued via a command queue.
 *
 * We use a global EventID space, where the LPI INTID corresponding to an
 * EventID is derived by adding `Lpi_intid_base` (8192) to the EventID.
 *
 *
 * To avoid deadlocks, locks may only be grabbed in the following order:
 *   1. If necessary, grab an LPI lock.
 *   2. If necessary, grab the `_device_alloc_lock`.
 *   3. If necessary, grab the `_cmd_queue_lock`.
 */
class Gic_its
{
public:
  enum
  {
    GITS_CTLR         = 0x0000,
    GITS_IIDR         = 0x0004,
    GITS_TYPER        = 0x0008,
    GITS_STATUSR      = 0x0040,
    GITS_UMSIR        = 0x0048,
    GITS_CBASER       = 0x0080,
    GITS_CWRITER      = 0x0088,
    GITS_CREADR       = 0x0090,
    GITS_BASER        = 0x0100,
    GITS_PIDR2        = 0xffe8,
    GITS_ITS_BASE     = 0x10000,
    GITS_TRANSLATER   = GITS_ITS_BASE + 0x0040,

    GITS_baser_num = 8,

    GITS_cmd_queue_align       = 0x10000,
    GITS_cmd_queue_size        = GITS_cmd_queue_align,
    GITS_cmd_queue_page_size   = 0x1000,
    GITS_cmd_queue_entry_size  = 32,
    GITS_cmd_queue_offset_mask = 0xfffe0,
  };

  struct Ctlr
  {
    Unsigned32 raw;
    explicit Ctlr(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER   ( 0,  0, enabled, raw);
    CXX_BITFIELD_MEMBER   ( 8,  8, umsi_irq, raw);
    CXX_BITFIELD_MEMBER_RO(31, 31, quiescent, raw);
  };

  struct Typer
  {
    Unsigned64 raw;
    explicit Typer(Unsigned64 v) : raw(v) {}
    CXX_BITFIELD_MEMBER_RO( 4,  7, itt_entry_size, raw);
    CXX_BITFIELD_MEMBER_RO( 8, 12, id_bits, raw);
    CXX_BITFIELD_MEMBER_RO(13, 17, dev_bits, raw);
    CXX_BITFIELD_MEMBER_RO(19, 19, pta, raw);
    CXX_BITFIELD_MEMBER_RO(24, 31, hcc, raw);
  };

  struct Cbaser
  {
    Unsigned64 raw;
    Cbaser() = default;
    explicit Cbaser(Unsigned64 v) : raw(v) {}
    CXX_BITFIELD_MEMBER          ( 0,  7, size, raw);
    CXX_BITFIELD_MEMBER          (10, 11, shareability, raw);
    CXX_BITFIELD_MEMBER_UNSHIFTED(12, 51, pa, raw);
    CXX_BITFIELD_MEMBER          (59, 61, cacheability, raw);
    CXX_BITFIELD_MEMBER          (63, 63, valid, raw);
  };

  struct Baser
  {
    Unsigned64 raw;
    Baser() = default;
    explicit Baser(Unsigned64 v) : raw(v) {}
    CXX_BITFIELD_MEMBER          ( 0,  7, size, raw);
    CXX_BITFIELD_MEMBER          ( 8,  9, page_size, raw);
    CXX_BITFIELD_MEMBER          (10, 11, shareability, raw);
    CXX_BITFIELD_MEMBER_UNSHIFTED(12, 47, pa, raw);
    CXX_BITFIELD_MEMBER_RO       (48, 52, entry_size, raw);
    CXX_BITFIELD_MEMBER_RO       (56, 58, type, raw);
    CXX_BITFIELD_MEMBER          (59, 61, cacheability, raw);
    CXX_BITFIELD_MEMBER          (62, 62, indirect, raw);
    CXX_BITFIELD_MEMBER          (63, 63, valid, raw);

    enum Type
    {
      Type_none       = 0,
      Type_device     = 1,
      Type_vpe        = 2,
      Type_collection = 4,
    };

    enum
    {
      Page_size_4k     = 0,
      Page_size_16k    = 1,
      Page_size_64k    = 2,

      Size_max        = 256,
    };
  };

  struct L1_entry
  {
    Unsigned64 raw;
    L1_entry() = default;
    explicit L1_entry(Unsigned64 v) : raw(v) {}
    CXX_BITFIELD_MEMBER_UNSHIFTED(12, 51, pa, raw);
    CXX_BITFIELD_MEMBER          (63, 63, valid, raw);

    enum { Size = 8 };
  };

  using Dev_id = unsigned;
  using Event_id = unsigned;
  using Icid = unsigned;

  enum
  {
    // One collection for each CPU (redistributor).
    Num_cols     = Config::Max_num_cpus,
    Invalid_icid = Num_cols + 1,
  };

  class Table
  {
  public:
    using Reg = Mmio_register_block::Reg_t<Unsigned64>;
    enum
    {
      // Arbitrarily chosen threshold for using two-level tables (64 KiB).
      Max_direct_size = 0x10000,
    };

    inline bool is_valid() const
    { return _mem.is_valid(); }

    inline Gic_mem::Mem_chunk const &mem() const
    { return _mem; }

    void alloc(Reg r, Typer typer);
    bool ensure_id_present(unsigned id);

    inline Baser::Type type() const
    { return _type; }

  private:
    Baser::Type _type = Baser::Type_none;
    Gic_mem::Mem_chunk _mem;
    unsigned _page_size = 0;
    unsigned _entry_size = 0;
    bool _indirect = false;
  };

  enum
  {
    Max_num_devs = 64,
  };

  class Device;
  struct Collection
  {
    Icid icid = Invalid_icid;
    Address redist_base;

    inline bool is_valid() const
    { return icid != Invalid_icid; }
  };

  struct Lpi
  {
    Lpi() : lock(Spin_lock<>::Unlocked) {}

    Event_id event_id;
    Device *device = nullptr;
    Collection const *col = nullptr;
    bool enabled = false;
    Spin_lock<> lock;

    inline void reset()
    {
      enabled = false;
      device = nullptr;
      col = nullptr;
    }
  };

  class Device : public cxx::H_list_item
  {
  public:
    explicit Device(Dev_id id, Gic_its &its) : _id(id), _its(its) {}

    ~Device()
    {
      if (_itt.is_valid())
        free_itt();
    }

    bool setup_itt();
    void free_itt();
    unsigned itt_size();

    void bind_lpi(Lpi &lpi, unsigned intid);
    void unbind_lpi(Lpi &lpi);

    inline Dev_id id()
    { return _id; }

    bool has_lpis()
    { return _lpi_count; }

  private:
    Dev_id _id;
    Gic_its &_its;
    Gic_mem::Mem_chunk _itt;
    unsigned _lpi_count = 0;
  };

  class Cmd
  {
  private:
    Unsigned64 raw0 = 0;
    Unsigned64 raw1 = 0;
    Unsigned64 raw2 = 0;
    Unsigned64 raw3 = 0;

  public:
    enum : unsigned { Size = GITS_cmd_queue_entry_size };

    enum Op
    {
      Op_movi    = 0x01,
      Op_sync    = 0x05,
      Op_mapd    = 0x08,
      Op_mapc    = 0x09,
      Op_mapti   = 0x0a,
      Op_inv     = 0x0c,
      Op_invall  = 0x0d,
      Op_discard = 0x0f,
    };

    Cmd() = default;
    explicit Cmd(Op cmd_op) { op() = cmd_op; };

    CXX_BITFIELD_MEMBER          ( 0,  7, op, raw0);
    CXX_BITFIELD_MEMBER          (32, 63, dev_id, raw0);

    CXX_BITFIELD_MEMBER          ( 0, 31, event_id, raw1);
    CXX_BITFIELD_MEMBER          (32, 63, intid, raw1);
    CXX_BITFIELD_MEMBER          ( 0,  4, itt_size, raw1);

    CXX_BITFIELD_MEMBER          ( 0, 15, icid, raw2);
    CXX_BITFIELD_MEMBER_UNSHIFTED(16, 50, rd_base, raw2);
    CXX_BITFIELD_MEMBER_UNSHIFTED( 8, 51, itt_addr, raw2);
    CXX_BITFIELD_MEMBER          (63, 63, valid, raw2);

    /**
     * This command retargets an already mapped event to a different
     * redistributor.
     */
    static Cmd movi(Dev_id dev_id, Event_id event_id, Icid icid)
    {
      Cmd cmd(Op_movi);
      cmd.dev_id() = dev_id;
      cmd.event_id() = event_id;
      cmd.icid() = icid;
      return cmd;
    }

    /**
     * This command ensures that the effects of all previous physical commands
     * associated with the specified redistributor are globally observable.
     */
    static Cmd sync(Address rd_base)
    {
      Cmd cmd(Op_sync);
      cmd.rd_base() = rd_base;
      return cmd;
    }

    /**
     * This command maps a DeviceID to an interrupt translation table (ITT).
     */
    static Cmd mapd(Dev_id dev_id, Address itt_addr, unsigned itt_size)
    {
      Cmd cmd(Op_mapd);
      cmd.dev_id() = dev_id;
      if (itt_addr)
        {
          // Associate DeviceID with ITT
          cmd.itt_addr() = itt_addr;
          cmd.itt_size() = itt_size - 1;
          cmd.valid() = true;
        }
      else
        {
          // Remove association for DeviceID
          cmd.valid() = false;
        }
      return cmd;
    }

    /**
     * This command maps a collection to a redistributor.
     */
    static Cmd mapc(Icid icid, Address rd_base)
    {
      Cmd cmd(Op_mapc);
      cmd.icid() = icid;
      if (rd_base)
        {
          // Map ICID to target redistributor
          cmd.rd_base() = rd_base;
          cmd.valid() = true;
        }
      else
        {
          // Remove mapping for ICID
          cmd.valid() = false;
        }
      return cmd;
    }

    /**
     * This command maps an event to an LPI targeted at the specified
     * redistributor.
     */
    static Cmd mapti(Dev_id dev_id, Event_id event_id, unsigned intid, Icid icid)
    {
      Cmd cmd(Op_mapti);
      cmd.dev_id() = dev_id;
      cmd.event_id() = event_id;
      cmd.intid() = intid;
      cmd.icid() = icid;
      return cmd;
    }

    /**
     * This command ensures that any caching done by the redistributors
     * associated with the specified event is consistent with the LPI
     * configuration tables held in memory.
     */
    static Cmd inv(Dev_id dev_id, Event_id event_id)
    {
      Cmd cmd(Op_inv);
      cmd.dev_id() = dev_id;
      cmd.event_id() = event_id;
      return cmd;
    }

    static Cmd invall(Icid icid)
    {
      Cmd cmd(Op_invall);
      cmd.icid() = icid;
      return cmd;
    }

    /**
     * This command removes the mapping for the specified event from the ITT and
     * resets the pending state of the corresponding LPI.
     */
    static Cmd discard(Dev_id dev_id, Event_id event_id)
    {
      Cmd cmd(Op_discard);
      cmd.dev_id() = dev_id;
      cmd.event_id() = event_id;
      return cmd;
    }
  };
  static_assert(sizeof(Cmd) == Cmd::Size);

  void init(Gic_cpu_v3 *gic_cpu, Address base, unsigned num_lpis);
  void cpu_init(Cpu_number cpu, Gic_redist const &redist);
  int bind_lpi_to_device(unsigned pin, Unsigned64 src, Irq_mgr::Msi_info *inf);
  void free_lpi(unsigned pin);
  void ack_lpi(unsigned pin)
  {
    assert(pin < _lpis.size());

    _gic_cpu->ack(to_intid(pin));
  }

  void mask_lpi(unsigned pin)
  {
    assert(pin < _lpis.size());

    Lpi &lpi = _lpis[pin];
    auto g = lock_guard(lpi.lock);

    lpi.enabled = false;
    update_lpi_config(pin, lpi);
  }

  void unmask_lpi(unsigned pin)
  {
    assert(pin < _lpis.size());

    Lpi &lpi = _lpis[pin];
    auto g = lock_guard(lpi.lock);

    lpi.enabled = true;
    update_lpi_config(pin, lpi);
  }

  void assign_lpi_to_cpu(unsigned pin, Cpu_number cpu);


private:
  void init_tables(Typer typer);
  void init_cmd_queue();
  unsigned cmd_queue_read_off()
  {
    return _its.read<Mword>(GITS_CREADR) & GITS_cmd_queue_offset_mask;
  }

  /**
   * \pre The _cmd_queue_lock must be held.
   */
  Gic_its::Cmd *alloc_cmd_slot()
  {
    unsigned write_off = _cmd_queue_write_off;
    unsigned next_write_off = (write_off + Cmd::Size) % GITS_cmd_queue_size;

    L4::Poll_timeout_counter i(5000000);
    while (i.test(next_write_off == cmd_queue_read_off()))
      Proc::pause();

    if (EXPECT_FALSE(i.timed_out()))
      {
        WARNX(Error, "ITS: Command slot allocation timed out!\n");
        return nullptr;
      }

    _cmd_queue_write_off = next_write_off;
    return _cmd_queue.virt_ptr<Cmd>() + (write_off / sizeof(Cmd));
  }

  /**
   * \pre The _cmd_queue_lock must be held.
   */
  bool enqueue_cmd(Cmd const &cmd)
  {
    Cmd *slot = alloc_cmd_slot();
    if (!slot)
      return false;

    *slot = cmd;
    _cmd_queue.make_coherent(slot, slot + 1);
    return true;
  }

  bool is_cmd_complete(unsigned cmd_off, unsigned num_cmds)
  {
    unsigned read_off = cmd_queue_read_off();
    if (read_off >= cmd_off)
      // read_off has passed cmd_off or cmd_off has wrapped around
      return read_off - cmd_off < GITS_cmd_queue_size - (num_cmds * Cmd::Size);
    else
      // read_off has not yet passed cmd_off or read_off has wrapped around
      return cmd_off - read_off > (num_cmds * Cmd::Size);
  }

  void send_cmd(Cmd const &cmd, Collection const *col = nullptr);
  Device *get_or_alloc_device(Dev_id dev_id);
  void unbind_lpi_from_device(Lpi &lpi);

  /**
   * \pre The lpi.lock must be held.
   */
  void update_lpi_config(unsigned pin, Lpi &lpi)
  {
    Gic_redist::enable_lpi(pin, lpi.enabled);

    if (lpi.device && lpi.col)
    {
      send_cmd(Cmd::inv(lpi.device->id(), lpi.event_id), lpi.col);
    }
  }

  Collection const *get_col(Cpu_number cpu) const
  {
    return &_cols[cxx::int_value<Cpu_number>(cpu)];
  }

  unsigned num_lpis() const
  {
    return _lpis.size();
  }

  unsigned to_intid(unsigned pin) const
  {
    return Gic_dist::Lpi_intid_base + pin;
  }


  Gic_cpu_v3 *_gic_cpu;
  Mmio_register_block _its;
  Spin_lock<> _lock;

  Collection _cols[Num_cols];

  using Lpi_vect = cxx::static_vector<Lpi>;
  Lpi_vect _lpis;

  Table _tables[GITS_baser_num];
  Table *_device_table;

  Gic_mem::Mem_chunk _cmd_queue;
  unsigned _cmd_queue_write_off;
  Spin_lock<> _cmd_queue_lock;

  unsigned _itt_entry_size;
  unsigned _max_device_id;

  using Device_list = cxx::H_list_bss<Device>;
  Device_list _devices;
  unsigned _num_devs;
  Spin_lock<> _device_alloc_lock;

  using Device_alloc = Kmem_slab_t<Device>;
  static Device_alloc device_alloc;
};

