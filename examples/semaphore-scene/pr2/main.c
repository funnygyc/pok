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


#include <core/thread.h>
#include <core/partition.h>
#include <types.h>
#include "activity.h"


int main ()
{
 uint8_t tid;
  pok_ret_t ret;
  pok_thread_attr_t     tattr;

  tattr.priority = 5;
  tattr.time_capacity = 1;
  tattr.deadline = 20000000;
  tattr.period = 50000000;
  tattr.weight = 5;
  tattr.entry = pinger_job1;

  ret = pok_thread_create(&tid , &tattr);
  printf("[P2] pok_thread_create (Eating) return=%d\n", ret);

  tattr.priority = 1;
  tattr.arrive_time = 0;
  tattr.time_capacity = 5;
  tattr.deadline = 100000000;
  tattr.period = 1000000000;
  tattr.weight = 1;
  tattr.entry = pinger_job2;

  ret = pok_thread_create(&tid , &tattr);
  printf("[P1] pok_thread_create (Entertainment) return=%d\n", ret);

  tattr.priority = 2;
  tattr.arrive_time = 0;
  tattr.time_capacity = 6;
  tattr.deadline = 80000000;
  tattr.period = 2000000000;
  tattr.weight = 2;
  tattr.entry = pinger_job3;

  ret = pok_thread_create(&tid , &tattr);
  printf("[P1] pok_thread_create (Housework) return=%d\n", ret);

  pok_partition_set_mode (POK_PARTITION_MODE_NORMAL);
  pok_thread_wait_infinite ();

   return (0);
}


