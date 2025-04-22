#pragma once

#include <cstddef>
#include "l4_types.h"

/**
 * The abstract interface for a text I/O console.
 *
 * This abstract interface can be implemented for virtually every
 * text input or output device.
 */
class Console
{
public:
  enum Console_state
  {
    DISABLED    =     0,
    INENABLED   =     1, ///< input channel of console enabled
    OUTENABLED  =     2, ///< output channel of console enabled
    ENABLED     =     INENABLED | OUTENABLED, ///< console fully enabled
    FAILED      = 0x200, ///< initialization failed
  };

  enum Console_attr
  {
    // universal attributes
    INVALID     =     0,
    OUT         =   0x1, ///< output to console is possible
    IN          =   0x2, ///< input from console is possible
    // attributes to identify a specific console
    DIRECT      =   0x4, ///< output to screen or input from keyboard
    UART        =   0x8, ///< output to/input from serial serial line
    PUSH        =  0x20, ///< input console
    GZIP        =  0x40, ///< gzip+uuencode output and sent to uart console
    BUFFER      =  0x80, ///< ring buffer
    DEBUG       = 0x100, ///< kdb interface
  };

  /**
   * modify console state
   */
  virtual void state(Mword new_state)
  {
    _state = new_state;
  }

  /**
   * Write a string of len characters to the output.
   * @param str the string to write (no zero termination is needed)
   * @param len the number of characters to write.
   *
   * This method must be implemented in every implementation, but
   * can simply do nothing for input only consoles.
   */
  virtual int write(char const *str, size_t len)
  {
    (void)str;
    return len;
  }

  /**
   * read a character from the input.
   * @param blocking if true getchar blocks til a character is available.
   *
   * This method must be implemented in every implementation, but
   * can simply return -1 for output only consoles.
   */
  virtual int getchar(bool blocking = true)
  {
    (void) blocking;
    return -1; /* no input */
  }

  /**
   * Is input available?
   *
   * This method can be implemented.
   * It must return -1 if no information is available, 
   * 1 if at least one character is available, and 0 if
   * no character is available.
   */
  virtual int char_avail() const
  {
    return -1; /* unknown */
  }


  /**
   * Console attributes.
   */
  virtual Mword get_attributes() const
  {
    return 0;
  }

  virtual ~Console() {}

  explicit Console(Console_state state) : _state(state) {}

  void add_state(Console_state state)
  { _state |= state; }

  void del_state(Console_state state)
  { _state &= ~state; }

  /**
   * get current console state
   */
  Mword state() const
  {
    return _state;
  }

  bool failed() const
  {
    return _state & FAILED;
  }

  void fail()
  {
    _state |= FAILED;
  }

  const char *str_mode() const
  {
    static char const * const mode_str[] =
      { "      ", "Output", "Input ", "InOut " };
    return mode_str[get_attributes() & (OUT|IN)];
  }

  const char *str_state() const
  {
    static char const * const state_str[] =
      { "Disabled       ", "Output disabled",
        "Input disabled ", "Enabled        " };
    if (!failed())
      return state_str[state() & ENABLED];
    else
      return "FAILED!        ";
  }

  const char *str_attr(Mword bit) const
  {
    static char const * const attr_str[] =
      { "Direct", "Uart", "<unk>", "Push", "Gzip", "Buffer", "Kdb" };

    return (bit < 2 || bit >= cxx::size(attr_str)+2) ? "???" : attr_str[bit-2];
  }

public:
  /// stdout for libc glue.
  static Console *stdout;
  /// stderr for libc glue.
  static Console *stderr;
  /// stdin for libc glue.
  static Console *stdin;

protected:
  Mword  _state;
};

