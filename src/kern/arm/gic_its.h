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
 * Because there can be multiple ITSs in the system, which might each be
 * responsible for a different subset of devices, the assignment of an LPI
 * to an ITS is not decided until user space binds the corresponding MSI to
 * a device. Thus, LPIs are not managed inside a Gic_its object, but in the
 * MSI interrupt controller (Gic_msi).
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
    Unsigned64 raw = 0;
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
    Unsigned64 raw = 0;
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
    Unsigned64 raw = 0;
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

  struct Rd_base
  {
    Unsigned64 raw = 0;

    CXX_BITFIELD_MEMBER_UNSHIFTED(16, 50, phys_base_addr, raw);
    CXX_BITFIELD_MEMBER          (16, 31, processor_nr, raw);
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

    inline Gic_mem const &mem() const
    { return _mem; }

    void alloc(Reg r, Typer typer);
    bool ensure_id_present(unsigned id);

    inline Baser::Type type() const
    { return _type; }

  private:
    Baser::Type _type = Baser::Type_none;
    Gic_mem _mem;
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
    Rd_base redist_base;

    inline bool is_valid() const
    { return icid != Invalid_icid; }
  };

  /**
   * LPI base class that contains all the state of an LPI relevant for the
   * ITS, and from which the MSI interrupt controller (Gic_msi) derives its LPI state
   * representation class.
   */
  class Lpi
  {
  public:
    Lpi() : lock(Spin_lock<>::Unlocked) { reset(); }

    // Index of this LPI, assigned by the MSI interrupt controller.
    unsigned index;
    // Coordinates concurrent operations on this LPI.
    mutable Spin_lock<> lock;

  private:
    friend class Gic_its;

    Device *device;
    Collection const *col;

    inline void reset()
    {
      device = nullptr;
      col = nullptr;
    }

    /**
     * We use a global EventID space, where the EventID of an LPI corresponds
     * to its index.
     *
     * With a per device EventID space we could save resources due to smaller
     * ITTs, but we would need a per device EventID allocation scheme, such as
     * a bitmap.
     * However, an ITT size tailored to the specific requirements of the device
     * is not possible because of the limitations of the Fiasco MSI userspace
     * API. Thus, the ITT size would decrease to the number of MSIs needed by
     * the device with the highest demand, instead of the sum of MSIs required
     * by all devices.
     */
    Event_id event_id() const { return index; }
    unsigned intid() const { return Gic_dist::Lpi_intid_base + index; }
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

    void bind_lpi(Lpi &lpi);
    void unbind_lpi(Lpi &lpi);

    inline Dev_id id()
    { return _id; }

    bool has_lpis()
    { return _lpi_count; }

  private:
    Dev_id _id;
    Gic_its &_its;
    Gic_mem _itt;
    unsigned _lpi_count = 0;
  };

  class Cmd
  {
  private:
    Unsigned64 raw[4] = { 0 };

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

    CXX_BITFIELD_MEMBER          ( 0,  7, op, raw[0]);
    CXX_BITFIELD_MEMBER          (32, 63, dev_id, raw[0]);

    CXX_BITFIELD_MEMBER          ( 0, 31, event_id, raw[1]);
    CXX_BITFIELD_MEMBER          (32, 63, intid, raw[1]);
    CXX_BITFIELD_MEMBER          ( 0,  4, itt_size, raw[1]);

    CXX_BITFIELD_MEMBER          ( 0, 15, icid, raw[2]);
    CXX_BITFIELD_MEMBER_UNSHIFTED(16, 50, rd_base, raw[2]);
    CXX_BITFIELD_MEMBER_UNSHIFTED( 8, 51, itt_addr, raw[2]);
    CXX_BITFIELD_MEMBER          (63, 63, valid, raw[2]);

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
    static Cmd sync(Rd_base rd_base)
    {
      Cmd cmd(Op_sync);
      cmd.rd_base() = rd_base.raw;
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
    static Cmd mapc(Icid icid, Rd_base rd_base, bool valid)
    {
      Cmd cmd(Op_mapc);
      cmd.icid() = icid;
      if (valid)
        {
          // Map ICID to target redistributor
          cmd.rd_base() = rd_base.raw;
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
  static_assert(sizeof(Cmd) == Cmd::Size, "Invalid size of Cmd");

  static void disable(void *base);

#ifdef CONFIG_ARM_GIC_MSI
  void init(Gic_cpu_v3 *gic_cpu, void *base, unsigned num_lpis);
  void cpu_init(Cpu_number cpu, Gic_redist const &redist);
  int bind_lpi_to_device(Lpi &lpi, Unsigned32 src, Irq_mgr::Msi_info *inf);
  void free_lpi(Lpi &lpi);

  /**
   * \pre The lpi.lock must be held.
   */
  void ack_lpi(Lpi &lpi)
  {
    // TODO: Acknowledging an LPI could and probably should be optimized by
    // moving it out of Gic_its into Gic_msi. Gic_msi would not even have to
    // lookup the Lpi, but instead could directly invoke
    // _gic_cpu->ack(Gic_dist::Lpi_intid_base + pin), see the implementation of
    // Lpi::intid().
    _gic_cpu->ack(lpi.intid());
  }

  /**
   * \pre The lpi.lock must be held.
   */
  void mask_lpi(Lpi &lpi)
  {
    update_lpi_config(lpi, false);
  }

  /**
   * \pre The lpi.lock must be held.
   */
  void unmask_lpi(Lpi &lpi)
  {
    update_lpi_config(lpi, true);
  }

  /**
   * \pre The lpi.lock must be held.
   */
  void assign_lpi_to_cpu(Lpi &lpi, Cpu_number cpu);

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
  void update_lpi_config(Lpi &lpi, bool enable)
  {
    Gic_redist::enable_lpi(lpi.intid() - Gic_dist::Lpi_intid_base, enable);

    if (lpi.device && lpi.col)
    {
      send_cmd(Cmd::inv(lpi.device->id(), lpi.event_id()), lpi.col);
    }
  }

  Collection const *get_col(Cpu_number cpu) const
  {
    return &_cols[cxx::int_value<Cpu_number>(cpu)];
  }

  unsigned num_lpis() const
  {
    return _num_lpis;
  }


  Gic_cpu_v3 *_gic_cpu;
  Mmio_register_block _its;
  Spin_lock<> _lock;

  Collection _cols[Num_cols];
  bool _redist_pta;

  unsigned _num_lpis;

  Table _tables[GITS_baser_num];
  Table *_device_table;

  Gic_mem _cmd_queue;
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
#endif
};

