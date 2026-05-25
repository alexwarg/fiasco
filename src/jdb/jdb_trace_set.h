#pragma once

#include <jdb_module.h>

class Jdb_set_trace : public Jdb_module
{
public:
  enum Mode { Off, Log, Log_to_buf, Use_slow_path };

  Jdb_set_trace();
  void ipc_tracing(Mode mode);

  Action_code action(int cmd, void *&args, char const *&fmt, int &) override;
  Cmd const *cmds() const override;
  int num_cmds() const override;

private:
  static char first_char;
  static char second_char;
};


