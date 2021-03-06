/**
 * @file
 *
 * @ingroup RTEMSImplClassic
 *
 * @brief This source file contains the implementation to support the Classic
 *   API in multiprocessing (MP) configurations.
 */

/*
 *  COPYRIGHT (c) 1989-1999.
 *  On-Line Applications Research Corporation (OAR).
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.org/license/LICENSE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <rtems/rtems/mp.h>
#include <rtems/score/mpciimpl.h>

void rtems_multiprocessing_announce ( void )
{
  _MPCI_Announce();
}

/* end of file */
