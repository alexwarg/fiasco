# The Fiasco.OC Microkernel Repository

This repository contains the source code of the L4Re microkernel also known as
the Fiasco.OC microkernel. User level applications are not included in this
package.

Fiasco is used to construct flexible systems that support running real-time,
time-sharing and virtualization workloads concurrently on one system. The
kernel scales from big and complex systems down to small and embedded
applications. It supports the following architectures:

| Architecture | 32 bit | 64 bit | Status            |
|:------------:|:------:|:------:|:-----------------:|
|      x86     |    x   |   x    | ![Build check][3] |
|      ARM     |    x   |   x    | ![Build check][4] |
|      MIPS    |    x   |   x    | ![Build check][5] |
|     RISC-V   |    x   |   x    | ![Build check][6] |

For a full list of the supported platforms and features see the [feature
list][1].

We welcome contributions to the microkernel. Please see our contributors guide
on [how to contribute][2].

[1]: https://l4re.org/fiasco/features.html
[2]: https://kernkonzept.com/L4Re/contributing/fiasco
[3]: https://github.com/kernkonzept/fiasco/actions/workflows/check_build_x86.yml/badge.svg?branch=master
[4]: https://github.com/kernkonzept/fiasco/actions/workflows/check_build_arm.yml/badge.svg?branch=master
[5]: https://github.com/kernkonzept/fiasco/actions/workflows/check_build_mips.yml/badge.svg?branch=master
[6]: https://github.com/kernkonzept/fiasco/actions/workflows/check_build_riscv.yml/badge.svg?branch=master

## Reporting vulnerabilities

We encourage responsible disclosure of vulnerabilities you may discover. Please
disclose them privately via **security@kernkonzept.com** to us.

# Differences: Upstream vs Alex's version

This section documents the conceptual differences between the upstream version
and Alex's version.

## Code Organization: Preprocess Removal

This version eliminates the Fiasco-specific "preprocess" tool entirely.
All `.cpp` files (which were input to preprocess) have been converted
to standard C++ `.cc` and `.h` files. This affects the entire codebase: kernel
core (context, thread, IPC), JDB, drivers, architecture-specific code, and
build infrastructure. The build now uses plain C++17 compilation without an
intermediate code generation step. Note, build system structure is still
kept the same.

## IPC Rework: Atomic Operations Replace DRQs

The IPC subsystem has been fundamentally reworked:

- **Cross-CPU IPC no longer uses DRQs.** The implementation is based on atomic
  operations on thread state and on the receiver's sender list. This eliminates
  the potentially unbounded DRQ operations that were also not handled correctly
  by timeouts.
- **Cross-CPU state changes use atomic ops instead of DRQs.** The old
  `xcpu_state_change` is replaced by normal atomic `state.change()` plus
  dedicated `xcpu_*_ready_enqueue` / `remote_ready_enqueue` operations.
- **Reply capability handling** is reworked: strict reset of the reply
  capability either when sending a reply or when the caller returns from IPC.
- IPC gate restart semantics changed: the whole syscall is restarted if woken
  up on a gate.

The DRQ-less IPC path significantly improves cross-CPU IPC performance while
local (same-CPU) IPC takes some performance hit.

## Missing Features Compared to Upstream

- **RISC-V architecture support** is not present.
- **AMP (Asymmetric Multi-Processing)** support is not present.
- **Alien thread feature** has been removed.

## JDB Coredump Module and CFI

A JDB module generates coredump files (base64-encoded mini-coredumps of TCB +
SP/IP values) for a given kernel thread. These can be loaded into GDB with the
kernel ELF binary to get full backtraces. The build system has been adjusted to
support including CFI (Call Frame Information) for debugging, while the default
build disables unwind tables (`-fno-asynchronous-unwind-tables`) for size.

## KIP: Pre-2025-06-09 Format

This version still uses the Fiasco.OC KIP (Kernel Info Page) format,
predating the upstream commit from 2025-06-09 (Change-Id:
Ibb57ff6932e3ee4e9db100c75352a8f3c924dda9, "[API] Kip: Merge and simplify kip
structures") which merged 32-bit and 64-bit KIP structures, removed historical
fields, and simplified mem_info. That change has not been integrated yet.

A KIP memory descriptor sub-type for passing the device tree (FDT) blob
location to the kernel has been added.

## Device Tree: Via KIP Memory Descriptor

Device tree information is passed to the kernel via a KIP memory descriptor
(with a dedicated sub-type for DT). The kernel has read-only FDT access via
a bundled libfdt. Platform configuration (GIC, timer, PSCI method) can be
derived from the device tree at boot time.

## UART Selection via Koptions

UART selection is driven by Koptions (via a CID in the options page), not
directly by device tree. The kernel UART subsystem has been decoupled from
specific drivers, with a registry-based approach for driver selection.

## AArch64 ABI: Syscall Register Assignment

The AArch64 syscall register assignment differs between the two versions.
Both use a 5-register `Syscall_frame` overlaid onto `Return_frame::r[0..4]`
(x0..x4), but the mapping of logical IPC arguments to physical registers
is different:

| Register | Kernkonzept upstream     | Alex's version             |
|:--------:|:------------------------:|:--------------------------:|
| x0       | tag                      | tag                        |
| x1       | from_spec                | utcb                       |
| x2       | ref (object selector)    | ref (object selector)      |
| x3       | timeout                  | timeout                    |
| x4       | /*utcb*/                 | from_spec                  |

The `utcb` and `from_spec` arguments are swapped between x1/x4.

## Map Items: Buffer Register Layout

The two versions differ in how the destination task for map operations is
specified in buffer registers:

**Alex's version** uses a `cap_br_idx` bitfield (bits 8..13) in
the buffer item's first word. This field encodes an index into the buffer
registers where a capability to the destination task is stored. The buffer
item itself remains 2 words (control word + flexpage). The `compound` bit
(bit 0) signals that a destination task capability should be looked up via
`cap_br_idx`.

**Kernkonzept upstream** uses a `forward_mappings` flag (bit 0 of the buffer
item). When set, the buffer item becomes 3 words: control word + flexpage +
task capability selector. The third word is an `L4_obj_ref` naming the
destination task. Classes are split: `L4_snd_item` (send items, always 2
words) and `L4_buf_item` (buffer items, 2 or 3 words depending on
`forward_mappings`). The upstream also removes `Obj_attribs`, `Memory_type`,
and `is_grant()` from the base `L4_msg_item` into the send-specific subclass
`L4_snd_item`.

In summary: this version encodes the destination task as an extra
capability reference stored at a BR index, keeping map items 2 words; the
upstream version appends the task reference as a third word in the buffer
item itself.

## Other ABI / API Differences

- Reply capabilities: this version supports only implicit reply
  capabilities. Upstream added and then refined explicit reply capability
  addressing.
- AArch64 syscall register assignment (Change-Id:
  I26e1d5eef3d3a140464d78ccbdfbdcafd1fe3fcb): upstream changed the register
  convention to be standard-ABI compliant for lazy dynamic linker resolution.
  Not merged; needs corresponding l4sys user-level change.
- ACPI RSDP/XSDP pointer via KIP (Change-Id:
  I67e9f59af066285ff72093700e154811e55f1f7f): upstream passes RSDP from
  bootstrap via KIP instead of searching for it. Not picked for ABI reasons.
- Factory output UTCB (Change-Id: Ic8faf47c82bbbd7f7024b21252e6b06b18eebd65):
  upstream allows factory operations to write additional return words to the
  UTCB. Not present in this version.
- Upstream msg item class split (L4_msg_item -> L4_snd_item + L4_buf_item with
  3-word buffer items): considered unnecessary restructuring, not merged.
- x86 VMX: upstream changed the ext_vcpu binary layout (added `nested_revision`
  field to `Vmx_user_info` at offset 0x200, shifting subsequent fields) and
  moved software-defined VMCS field indexes from 0x28**4**x/0x68**3**e to
  0x28**8**x/0x68**8**x to avoid collisions with newer Intel CPUs. Upstream
  also adds nested virtualization fields (`Sw_nat_arg0..3`). None of this is
  merged.
- **Generic IRQ forwarding / bind_vcpu** (Change-Id:
  I91aca9e1d70104ca18760207810febe1822ca08e): upstream adds forwarding logic
  into the existing IRQ path. Missing in Alex's version.

## Design Disagreements with Upstream

- **Edge-triggered IRQ buffering** (Change-Id:
  Ic364f34afd08e3397cc61c0015c46415a5cd8b8e): upstream buffers edge-triggered
  IRQs when no thread is bound. A driver must check conditions after attaching;
  this allows a simpler kernel.
- **Irq_sender existence lock** (Change-Id:
  I0a61ca52006874dc9270ffce7e01bfb7d694019f): upstream adds reference counting
  to prevent deletion during bind/detach. The existence lock already prevents
  deletion by design, considering the rechecking pattern applied to the locks.
- **map_util overmap** (Change-Id:
  I6d8027f5cd99074bc49caa1ef37be0f64b62465a): upstream adds a check in
  map_util for empty mappings after overmap. This version handles
  this in lookup_dst_src (like memory mappings), see Change-Id:
  I6b1af7896de9921e7f68256281cd64042327cf50.
- **Doorbell IRQ support** (Change-Id:
  I8acf4e5d39a9f1487cd5633a14858d261601a438): upstream stores IRQs as
  pointers to Irq_base in the thread. This version considers this
  a race condition on deletion; IRQs should be stored as capabilities.

# Building

The L4Re Microkernel can be built using a recent version of gcc (>=11) or
clang (>=10), GNU binutils, GNU make and Perl.

Change to the top-level directory of this project and create a build directory
by typing
```
$ make BUILDDIR=/path/to/build
```

Change to the newly created build directory. You can now modify the default
configuration by typing
```
$ make menuconfig
```

Make the desired changes, save and exit the configuration. Now you can build
the kernel by typing
```
$ make
```

Use ```-j``` option to make as you see fit. If the
build completed successfully you can find the kernel binary as *fiasco* in
the build directory.

For further information please refer to our [detailed build
instructions on l4re.org](https://l4re.org/getting_started/make.html#building-the-l4re-microkernel).

# License

The core L4Re microkernel is licensed under the MIT license, with 3rd-party
code being BSD 2-clause and other parts of debugging functionality being
licensed under other licenes. Please consult the LICENSE.spdx for details.
