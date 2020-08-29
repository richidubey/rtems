set print elements 0
set print pretty on
set pagination off
set confirm off
tar remote :1234
load
info threads
define reset
  monitor system_reset
  load
end
define new
  make
  monitor system_reset
  load
  c
end
b _ARM_Exception_default
b _ARMV4_Exception_data_abort_default
b bsp_interrupt_handler_default
b bsp_reset
b _Terminate
b Init
reset
