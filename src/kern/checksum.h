#pragma once

namespace Checksum {
  // calculate simple checksum over kernel text section and read-only data
  unsigned get_checksum_ro();
  bool check_ro();
  // calculate simple checksum over kernel data section
  unsigned get_checksum_rw();
}

