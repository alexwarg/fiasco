#pragma once


inline
void Io::iodelay()
{}

inline
Unsigned8  Io::in8(unsigned long port)
{
  return *reinterpret_cast<volatile Unsigned8 *>(port);
}

inline
Unsigned16 Io::in16( unsigned long port )
{
  return *reinterpret_cast<volatile Unsigned16 *>(port);
}

inline
Unsigned32 Io::in32(unsigned long port)
{
  return *reinterpret_cast<volatile Unsigned32 *>(port);
}

inline
void Io::out8 (Unsigned8  val, unsigned long port)
{
  *reinterpret_cast<volatile Unsigned8 *>(port) = val;
}

inline
void Io::out16( Unsigned16 val, unsigned long port)
{
  *reinterpret_cast<volatile Unsigned16 *>(port) = val;
}

inline
void Io::out32(Unsigned32 val, unsigned long port)
{
  *reinterpret_cast<volatile Unsigned32 *>(port) = val;
}


