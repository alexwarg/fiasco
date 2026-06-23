#pragma once

#include "static_init.h"
#include "jdb_module.h"
#include <stdio.h>

/**
 * The core of the modularized Jdb.
 * @see Jdb_module
 * @see Jdb_category
 *
 * This class provides the core functions for handling
 * Jdb_modules and providing them with the right input.
 *
 */
class Jdb_core
{
public:
  typedef int (Input_fmt)(char fmt, int *size, char const *cmd_str, void *buf);

  /**
   * The command structure for Jdb_core.
   *
   * This structure consists of a pointer to the Jdb_module
   * and a Jdb_module::Cmd structure. It is used in exec_cmd()
   * and returned from has_cmd().
   */
  struct Cmd
  {
    /**
     * Pointer to the module providing this command.
     */
    Jdb_module            *mod;

    /**
     * The Jdb_module::Cmd structure, describing the command.
     *
     * If this is a null pointer the command is invalid.
     * @see Jdb_module
     * @see Jdb_module::Cmd
     */
    Jdb_module::Cmd const *cmd;

    /**
     * Create a Jdb_core::Cmd.
     * @param _mod the Jdb_module providing the command.
     * @param _cmd the command structure (see Jdb_module::Cmd).
     */
    Cmd(Jdb_module *_mod = nullptr, Jdb_module::Cmd const *_cmd = nullptr)
      : mod(_mod), cmd(_cmd)
    {}
  };

  /**
   * Get the command structure accoring to the given name.
   * @param cmd the command to look for.
   * @return A valid Cmd structure if cmd was found, or a
   *         Cmd structure where Cmd::cmd is a null pointer if
   *         no module provides such a command.
   */
  static Cmd has_cmd(char const *cmd);

  /**
   * Execute the command according to cmd.
   * @param cmd the command structure (see Jdb_core::Cmd), which
   *        describes the command to execute.
   * @return 0 if Jdb_module::action() returned LEAVE
   *         1 if Jdb_module::action() returned NOTHING
   *         2 if Jdb_module::action() returned GO_BACK (KEY_HOME entered)
   *         3 if the input was aborted (KEY_ESC entered) or was invalid
   *
   * This method is actually responsible for reading the input
   * with respect to the commands format string and calling
   * the Jdb_module::action() method after that.
   *
   */
  static int exec_cmd(Cmd const cmd, char const *str, int push_next_char = -1);

  /**
   * Overwritten getchar() to be able to handle next_char.
   */
  static int getchar(void);

  /**
   * Call this function every time a `\n' is written to the
   *        console and it stops output when the screen is full.
   * @return 0 if user wants to abort the output (escape or 'q' pressed)
   */
  static int new_line(unsigned &line);

  static void prompt_start();
  static void prompt_end();
  static void prompt();
  static int  prompt_len();
  static int set_prompt_color(char v);

  /**
   * Like strlen but do not count ESC sequences.
   */
  static int print_len(const char *s);
  static int invisible_len(char const *s);
  static char esc_prompt[];

  static void (*wait_for_input)();

  static bool add_fmt_handler(char fmt, Input_fmt* hdlr);
  static unsigned print_alternatives(char const *prefix);
  static Cmd complete_cmd(char const *prefix, bool &multi_match);
  static int cmd_getchar(char const *&str);

  static void print_prompt() { prompt_start(); prompt(); prompt_end(); }

  static void cmd_putchar(int c)
  { if (short_mode) putchar(c); }

  static int get_ansi_color(char c)
  {
    switch(c)
      {
      case 'N': case 'n': return 30;
      case 'R': case 'r': return 31;
      case 'G': case 'g': return 32;
      case 'Y': case 'y': return 33;
      case 'B': case 'b': return 34;
      case 'M': case 'm': return 35;
      case 'C': case 'c': return 36;
      case 'W': case 'w': return 37;
      default:  return 0;
      }
  }


public:
  static bool short_mode;

private:
  static unsigned match_len(char const *a, char const *b, unsigned l);

  static int  next_char;
  static Input_fmt *_fmt_list[26];
};

#define JDB_ANSI_black        "30"
#define JDB_ANSI_gray         "30;1"
#define JDB_ANSI_red          "31"
#define JDB_ANSI_lightred     "31;1"
#define JDB_ANSI_green        "32"
#define JDB_ANSI_lightgreen   "32;1"
#define JDB_ANSI_brown        "33"
#define JDB_ANSI_yellow       "33;1"
#define JDB_ANSI_blue         "34"
#define JDB_ANSI_lightblue    "34;1"
#define JDB_ANSI_magenta      "35"
#define JDB_ANSI_lightmagenta "35;1"
#define JDB_ANSI_cyan         "36"
#define JDB_ANSI_lightcyan    "36;1"
#define JDB_ANSI_white        "37"
#define JDB_ANSI_brightwhite  "37;1"
#define JDB_ANSI_default      ""

#define JDB_ANSI_COLOR(color)   "\033[" JDB_ANSI_##color "m"

#define JDB_ANSI_END          "\033[m"

