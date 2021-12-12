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
#include <core/time.h>
#include "activity.h"
#include <core/partition.h>
#include <core/mutex.h>


extern uint8_t sid;
uint8_t val;

void *pinger_job1() {
  int flag = 1;
  while (1) {
    if(flag == 1){
      printf("P1T1\n");
      flag = 0;
    }
    
    //pok_thread_sleep(20000000);
  }
}

void *pinger_job2() {
  int flag = 1;
  while (1) {
    if(flag == 1){
      printf("P1T2\n");
      flag = 0;
    }
    //pok_thread_sleep(20000000);
  }
}
void *pinger_job3() {
  int flag = 1;
  while (1) {
    if(flag == 1){
      printf("P1T3\n");
      flag = 0;
    }
    
    //pok_thread_sleep(20000000);
  }
}

static uint64_t seed = 1;

static void srand(uint64_t new_seed)
{
   seed = new_seed;
}

static uint64_t rand()
{
   uint64_t res;
   seed = seed * 123456789 + 9876;
   res = (uint64_t)((seed / 65536) % 32768);
   return res;
}



void *pinger_job4() {
  uint32_t tid;
  pok_ret_t ret;
  pok_thread_attr_t tattr;

  uint64_t time=111;
  //uint64_t time1;
  //ret = pok_time_get(&time);
   //if (ret != 0)
   //   printf("[Random Generator] pok_time_get failed\n");
  srand(time);

  uint8_t flag = 0;
  uint64_t sleeptime = 184380000ULL;
  sleeptime = (uint64_t)(rand()%5 + 5) * sleeptime;
  while (1) {
    //pok_time_get(&time1);
    //if(flag == 0 && sleeptime <= time1-time ){
    if(flag == 0){
      flag = 1;

      tattr.priority = 3 + rand()%2;
      tattr.period = 18438000000ULL;
      tattr.time_capacity = 2 + rand()%2;
      tattr.deadline = 10 + rand()%2;
      tattr.weight = 2 + rand()%2;
      //tattr.stack_size = 2;
      tattr.entry = pinger_job5;
      tattr.processor_affinity = 0;
      
      ret = pok_thread_create(&tid, &tattr);
      printf("[P1] random_pok_thread_create  return=%d\n", ret);
      printf("active time : %lld\n", sleeptime / 184380000ULL);
      printf("priority = %d, time_capacity = %lld, deadline = %lld, weight = %ld\n", 
           tattr.priority, tattr.time_capacity, tattr.deadline,tattr.weight);

    }
    
  } 

}

void *pinger_job5() {
  int flag = 1;
  while (1) {
    if(flag == 1){
      printf("P1T5\n");
      flag = 0;
    }
    
    //pok_thread_sleep(20000000);
  }
}
