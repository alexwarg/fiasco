#include <boot_info.h>
#include <checksum.h>

// these members needs to be initialized with some
// data to go into the data section and not into bss
unsigned Boot_info::_checksum_ro   = 15;
unsigned Boot_info::_checksum_rw   = 16;


void
Boot_info::reset_checksum_ro(void)
{
  set_checksum_ro(Checksum::get_checksum_ro());
}
