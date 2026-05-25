
#include "jdb_screen.h"

unsigned int Jdb_screen::_height         = 25; // default for native
unsigned int Jdb_screen::_width          = 80; // default
bool         Jdb_screen::_direct_enabled = true;

const char *Jdb_screen::Mword_adapter    = "~~~~~~~~~~~~~~~~";
const char *Jdb_screen::Mword_not_mapped = "----------------";
const char *Jdb_screen::Mword_blank      = "                ";
const char *Jdb_screen::Line             = "------------------------------------"
                                           "-----------------------------------";
