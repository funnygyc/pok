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
#define TIME_INTERVAL 10000000

void* pinger_job1 ()
{
   while (1)
   {
      printf ("P2 Eating\n");
      pok_thread_sleep(10000);
   }
}

void* pinger_job2 ()
{
   while (1)
   {
      printf ("P2 Entertainment\n");
      pok_thread_sleep(10000);
   }
}

void* pinger_job3 ()
{
   while (1)
   {
      printf ("P2 Housework\n");
      pok_thread_sleep(10000);
   }
}
