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

#include "activity.h"
#include <core/partition.h>
#include <core/semaphore.h>
#include <core/thread.h>
#include <libc/stdio.h>
#include <types.h>
#define TIME_SLICE 184380000
uint8_t sid;

/*int main() {
  uint32_t tid;
  pok_ret_t ret;
  pok_thread_attr_t tattr;

  ret = pok_sem_create(&sid, 0, 2, POK_QUEUEING_DISCIPLINE_DEFAULT);
  printf("[P1] pok_sem_create return=%d, mid=%d\n", ret, sid);

  tattr.priority = 44;
  tattr.entry = pinger_job1;
  tattr.time_capacity = 3;
  tattr.deadline = 9;
  tattr.period = 10;
  //tattr.weight = 9;
  //tattr.processor_affinity = 0;

  ret = pok_thread_create(&tid, &tattr);
  printf("[P1] pok_thread_create (1) return=%d\n", ret);

  tattr.priority = 43;
  tattr.entry = pinger_job2;
  tattr.time_capacity = 3;
  tattr.deadline = 6;
  tattr.period = 10;
  //tattr.weight = 9;

  ret = pok_thread_create(&tid, &tattr);
  printf("[P1] pok_thread_create (2) return=%d\n", ret);

  pok_partition_set_mode(POK_PARTITION_MODE_NORMAL);
  pok_thread_wait_infinite();

  return (0);
}*/

int main() {
  uint32_t tid;
  pok_ret_t ret;
  pok_thread_attr_t tattr;

  ret = pok_sem_create(&sid, 0, 50, POK_QUEUEING_DISCIPLINE_DEFAULT);
  printf("[P1] pok_sem_create return=%d, mid=%d\n", ret, sid);

  tattr.priority = 3;
  tattr.period = 100ULL * TIME_SLICE;
  tattr.time_capacity = 2;
  tattr.deadline = 10;
  tattr.weight = 2;
  tattr.stack_size = 2;
  tattr.entry = pinger_job1;
  tattr.processor_affinity = 0;
  
  ret = pok_thread_create(&tid, &tattr);
  printf("[P1] pok_thread_create (1) return=%d\n", ret);

  tattr.priority = 1;
  tattr.period = 100ULL * TIME_SLICE;
  tattr.time_capacity = 3;
  tattr.deadline = 9;
  tattr.weight = 3;
  tattr.stack_size = 3;
  tattr.entry = pinger_job2;

  ret = pok_thread_create(&tid, &tattr);
  printf("[P1] pok_thread_create (2) return=%d\n", ret);

  tattr.priority = 2;
  tattr.period = 100ULL * TIME_SLICE;
  tattr.time_capacity = 4;
  tattr.deadline = 11;
  tattr.weight = 4;
  tattr.stack_size = 4;
  tattr.entry = pinger_job3;
  tattr.processor_affinity = 0;

  ret = pok_thread_create(&tid, &tattr);
  printf("[P1] pok_thread_create (3) return=%d\n", ret);

  tattr.priority = 4;
  tattr.period = 100ULL * TIME_SLICE;
  tattr.time_capacity = 5;
  tattr.deadline = 12;
  tattr.weight = 5;
  tattr.stack_size = 5;
  tattr.entry = pinger_job4;
  tattr.processor_affinity = 0;

  ret = pok_thread_create(&tid, &tattr);
  printf("[P1] pok_thread_create (4) return=%d\n", ret);

  pok_partition_set_mode(POK_PARTITION_MODE_NORMAL);
  pok_thread_wait_infinite();

  return (0);
}
