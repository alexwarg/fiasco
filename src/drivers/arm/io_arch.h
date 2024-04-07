#pragma once


inline
void Io::iodelay()
{}

inline
Unsigned8  Io::in8(unsigned long port)
{
  return *(volatile Unsigned8 *)port;
}

inline
Unsigned16 Io::in16( unsigned long port )
{
  return *(volatile Unsigned16 *)port;
}

inline
Unsigned32 Io::in32(unsigned long port)
{
  return *(volatile Unsigned32 *)port;
}

inline
void Io::out8 (Unsigned8  val, unsigned long port)
{
  *(volatile Unsigned8 *)port = val;
}

inline
void Io::out16( Unsigned16 val, unsigned long port)
{
  *(volatile Unsigned16 *)port = val;
}

inline
void Io::out32(Unsigned32 val, unsigned long port)
{
  *(volatile Unsigned32 *)port = val;
}


