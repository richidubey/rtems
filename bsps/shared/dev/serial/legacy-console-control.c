/**
 * @file
 *
 * @ingroup Console
 *
 * @brief Generic libchip console io_ctl extension
 */


/*
 *  This file is an extension of the generic console driver
 *  shell used by all console drivers using libchip.
 *
 *  COPYRIGHT (c) 1989-2011.
 *  On-Line Applications Research Corporation (OAR).
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.org/license/LICENSE.
 */

#include <bsp.h>
#include <rtems/libio.h>
#include <stdlib.h>
#include <assert.h>
#include <termios.h>

#include <rtems/termiostypes.h>
#include <rtems/console.h>
#include <libchip/serial.h>
#include "legacy-console.h"

/*
 *  console_control
 *
 *  this routine uses the termios driver to process io
 */
rtems_device_driver console_control(
  rtems_device_major_number major,
  rtems_device_minor_number minor,
  void                    * arg
)
{
  return rtems_termios_ioctl (arg);
}
