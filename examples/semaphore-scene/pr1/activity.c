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
#include <core/semaphore.h>
#include <core/time.h>
#include <types.h>
#include "activity.h"

#define TIME_INTERVAL 10000000

extern uint8_t sid;
extern uint8_t tid;

extern uint8_t add_thread_num;

uint8_t val;

void* pinger_job1 ()
{
   pok_ret_t ret;
   pok_thread_attr_t     tattr;

   pok_time_t time_now;

   while (1)
   {
      ret = pok_time_gettick(&time_now);

      printf ("P1 Project A: I will signal semaphores\n");
      ret = pok_sem_signal (sid);
      printf ("P1 Project A: pok_sem_signal, ret=%d\n", ret);

      if(add_thread_num == 1 && time_now > 40000000){
         tattr.priority = 5;
         tattr.time_capacity = 1;
         tattr.deadline = 20000000;
         tattr.period = 50000000;
         tattr.weight = 5;
         tattr.entry = pinger_job4;

         ret = pok_partition_thread_add(&tid , &tattr);
         printf("[P1] pok_thread_create (Eating) return=%d\n", ret);
         
         add_thread_num = add_thread_num + 1;
      }

      if(add_thread_num == 0 && time_now > 30000000){
         tattr.priority = 4;
         tattr.time_capacity = 1;
         tattr.deadline = 50000000;
         tattr.period = 150000000;
         tattr.weight = 4;
         tattr.entry = pinger_job5;

         ret = pok_partition_thread_add(&tid , &tattr);
         printf("[P1] pok_thread_create (Project C) return=%d\n", ret);
         
         add_thread_num = add_thread_num + 1;
      }

      pok_thread_sleep (10000);
   }
}


void* pinger_job2 ()
{
   pok_ret_t ret;
   pok_thread_attr_t     tattr;

   pok_time_t time_now; 

   while (1)
   {
      ret = pok_time_gettick(&time_now);

      printf ("P1 Project B: I will signal semaphores\n");
      ret = pok_sem_signal (sid);
      printf ("P1 Project B: pok_sem_signal, ret=%d\n", ret);

      if(add_thread_num == 1 && time_now > 40000000){
         tattr.priority = 5;
         tattr.time_capacity = 1;
         tattr.deadline = 20000000;
         tattr.period = 50000000;
         tattr.weight = 5;
         tattr.entry = pinger_job4;

         ret = pok_partition_thread_add(&tid , &tattr);
         printf("[P1] pok_thread_create (Eating) return=%d\n", ret);
         
         add_thread_num = add_thread_num + 1;
      }

      if(add_thread_num == 0 && time_now > 30000000){
         tattr.priority = 4;
         tattr.time_capacity = 1;
         tattr.deadline = 50000000;
         tattr.period = 150000000;
         tattr.weight = 4;
         tattr.entry = pinger_job5;

         ret = pok_partition_thread_add(&tid , &tattr);
         printf("[P1] pok_thread_create (Project C) return=%d\n", ret);
         
         add_thread_num = add_thread_num + 1;
      }

      pok_thread_sleep (10000);
   }
}

void* pinger_job3 ()
{
   pok_ret_t ret;
   pok_thread_attr_t     tattr;

   pok_time_t time_now;
   
   while (1)
   {
      printf ("P1 Meeting\n");
      ret = pok_time_gettick(&time_now);

      if(add_thread_num == 1 && time_now > 40000000){
         tattr.priority = 5;
         tattr.time_capacity = 1;
         tattr.deadline = 20000000;
         tattr.period = 50000000;
         tattr.weight = 5;
         tattr.entry = pinger_job4;

         ret = pok_partition_thread_add(&tid , &tattr);
         printf("[P1] pok_thread_create (Eating) return=%d\n", ret);
         
         add_thread_num = add_thread_num + 1;
      }

      if(add_thread_num == 0 && time_now > 30000000){
         tattr.priority = 4;
         tattr.time_capacity = 1;
         tattr.deadline = 50000000;
         tattr.period = 150000000;
         tattr.weight = 4;
         tattr.entry = pinger_job5;

         ret = pok_partition_thread_add(&tid , &tattr);
         printf("[P1] pok_thread_create (Project C) return=%d\n", ret);
         
         add_thread_num = add_thread_num + 1;
      }
      pok_thread_sleep (10000);
   }
}

void* pinger_job4 ()
{
   while (1)
   {
      printf ("P1 Eating\n");
      pok_thread_sleep (10000);
   }
}

void* pinger_job5 ()
{
   pok_ret_t ret;

   while (1)
   {
      printf ("P1 Project C: I will wait for the semaphores\n");
      ret = pok_sem_wait (sid, 0);
      printf ("P1 Project C: pok_sem_wait, ret=%d\n", ret);

      pok_thread_sleep (10000);
   }
}
