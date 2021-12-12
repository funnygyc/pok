/*
 *                               POK header
 *
 * The following file is a part of the POK project. Any modification should
 * be made according to the POK licence. You CANNOT use this file or a part
 * of a file for your own project.
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2021 POK team
 */

#include <core/semaphore.h>
#include <core/thread.h>
#include <libc/stdio.h>
#include <types.h>

extern uint8_t sid;
uint8_t val;

void *pinger_job1() {
  while (1) {
    printf("P1T1\n");
    pok_thread_sleep(20000000);
  }
}

void *pinger_job2() {
  while (1) {
    printf("P1T2\n");
    pok_thread_sleep(20000000);
  }
}
