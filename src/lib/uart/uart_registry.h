#pragma once

#define LIBUART_REGISTER_UART_FACTORY3(type, ids, id)     \
  __attribute__((section(".uart_registry"), used))         \
  static L4::Uart_factory_t<type> const _factory_##id{ids};

#define LIBUART_REGISTER_UART_FACTORY2(type, ids, id) \
  LIBUART_REGISTER_UART_FACTORY3(type, ids, id)

#define LIBUART_REGISTER_UART_FACTORY(type, ids) \
  LIBUART_REGISTER_UART_FACTORY2(type, ids, __COUNTER__)

