/*
 *  writev() - POSIX 1003.1 - Write a Vector
 *
 *  OpenGroup URL:
 *
 *  http://www.opengroup.org/onlinepubs/009695399/functions/writev.html
 *
 *  COPYRIGHT (c) 1989-2011.
 *  On-Line Applications Research Corporation (OAR).
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.com/license/LICENSE.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/uio.h>

#include <rtems/libio_.h>

ssize_t writev(
  int                 fd,
  const struct iovec *iov,
  int                 iovcnt
)
{
  ssize_t        total;
  int            v;
  rtems_libio_t *iop;

  total = rtems_libio_iovec_eval( fd, iov, iovcnt, LIBIO_FLAGS_WRITE, &iop );

  if ( total > 0 ) {
    /*
     *  Now process the writev().
     */
    total = 0;
    for ( v = 0 ; v < iovcnt ; v++ ) {
      ssize_t bytes = ( *iop->pathinfo.handlers->write_h )(
        iop,
        iov[ v ].iov_base,
        iov[ v ].iov_len
      );

      if ( bytes < 0 )
        return -1;

      total += bytes;

      if ( bytes != iov[ v ].iov_len )
        break;
    }
  }

  return total;
}
