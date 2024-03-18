#pragma once

void set_exit_question(void (*eq)(void));

[[noreturn]]
void terminate(int exit_value);

