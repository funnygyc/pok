/*
 *                               POK header
 * 
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team 
 *
 * Created by julien on Thu Jan 15 23:34:13 2009 
 */


#include <libc/stdio.h>
#include <core/thread.h>
#include <core/partition.h>
#include <core/semaphore.h>
#include <types.h>
#include "activity.h"

uint8_t sid;
uint8_t tid;
uint8_t add_thread_num;

int main ()
{
  pok_ret_t ret;
  pok_thread_attr_t     tattr;

  add_thread_num = 0;

  ret = pok_sem_create(&sid , 0, 2, POK_SEMAPHORE_DISCIPLINE_FIFO);
  printf("[P1] pok_sem_create return=%d, mid=%d\n", ret, sid);

  tattr.priority = 3;
  tattr.time_capacity = 3;
  tattr.deadline = 80000000;
  tattr.period = 100000000;
  tattr.weight = 3;
  tattr.entry = pinger_job1;

  ret = pok_thread_create(&tid , &tattr);
  printf("[P1] pok_thread_create (Project A) return=%d\n", ret);

  tattr.priority = 2;
  tattr.time_capacity = 2;
  tattr.deadline = 60000000;
  tattr.period = 100000000;
  tattr.weight = 2;
  tattr.entry = pinger_job2;

  ret = pok_thread_create(&tid , &tattr);
  printf("[P1] pok_thread_create (Project B) return=%d\n", ret);

  tattr.priority = 5;
  tattr.time_capacity = 1;
  tattr.deadline = 20000000;
  tattr.period = 50000000;
  tattr.weight = 5;
  tattr.entry = pinger_job3;

  ret = pok_thread_create(&tid , &tattr);
  printf("[P1] pok_thread_create (Meeting) return=%d\n", ret);

  pok_partition_set_mode (POK_PARTITION_MODE_NORMAL);
  pok_thread_wait_infinite ();

  return (0);
}
