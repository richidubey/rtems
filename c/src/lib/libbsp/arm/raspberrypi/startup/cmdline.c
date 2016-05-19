/**
 * @file
 *
 * @ingroup raspberrypi
 *
 * @brief mailbox support.
 */
/*
 * Copyright (c) 2015 Yang Qiao
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *
 *  http://www.rtems.org/license/LICENSE
 *
 */

#include <bsp.h>
#include <bsp/vc.h>

#define MAX_CMDLINE_LENGTH 1024
static int rpi_cmdline_ready;
static char rpi_cmdline_cached[MAX_CMDLINE_LENGTH];
static bcm2835_get_cmdline_entries rpi_cmdline_entries;

const char *rpi_cmdline_get_raw(void)
{
  memset(&rpi_cmdline_entries, 0, sizeof(rpi_cmdline_entries));
  bcm2835_mailbox_get_cmdline(&rpi_cmdline_entries);
  return rpi_cmdline_entries.cmdline;
}

const char *rpi_cmdline_get_cached(void)
{
  if (!rpi_cmdline_ready) {
    const char *line = rpi_cmdline_get_raw();
    strncpy(rpi_cmdline_cached, line, MAX_CMDLINE_LENGTH - 1);
    rpi_cmdline_cached[MAX_CMDLINE_LENGTH - 1] = 0;
    rpi_cmdline_ready = 1;
  }
  return rpi_cmdline_cached;
}

const char *rpi_cmdline_get_arg(const char* arg)
{
  return strstr (rpi_cmdline_get_cached(), arg);
}
