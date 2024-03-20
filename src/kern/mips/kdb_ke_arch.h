#pragma once

void kdb_ke(char const *msg) asm ("kern_kdebug_cstr_entry");
void kdb_ke_nstr(char const *msg, unsigned len) asm ("kern_kdebug_nstr_entry");
void kdb_ke_sequence(char const *msg, unsigned len) asm ("kern_kdebug_sequence_entry");
void kdb_ke_ipi() asm("kern_kdebug_ipi_entry");
