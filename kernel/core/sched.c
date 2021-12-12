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

/**
 **\\file   sched.c
 **\\brief  Function for partitions and kernel scheduling
 **\\author Julien Delange
 */

#include <arch.h>
#include <assert.h>
#include <types.h>

#include <core/sched.h>
#include <core/thread.h>
#include <core/time.h>

#include <arch.h>
#include <arch/x86/ipi.h>
#include <core/partition.h>

#ifdef POK_NEEDS_MIDDLEWARE
#include <middleware/port.h>
#endif

#include <dependencies.h>

#include <core/debug.h>
#include <core/error.h>
#include <core/instrumentation.h>

#ifdef POK_NEEDS_LOCKOBJECTS
#include <core/lockobj.h>

extern pok_lockobj_t pok_partitions_lockobjs[];
#endif

extern pok_thread_t pok_threads[];

#if defined(POK_NEEDS_DEBUG)
static const char *state_names[] = {
    "stopped",      "runnable", "waiting", "lock", "waiting next activation",
    "delayed start"};
#endif

extern pok_partition_t pok_partitions[];

/**
 * \brief The variable that contains the value of partition currently being
 * executed
 */
uint8_t pok_current_partition;
rendezvous fence, barr;

void pok_sched_partition_switch();

#if defined(POK_NEEDS_PORTS_SAMPLING) || defined(POK_NEEDS_PORTS_QUEUEING)
extern void pok_port_flushall(void);
extern void pok_port_flush_partition(uint8_t);
#endif

int current_weight = 0;
#define SPACE_CAPACITY 100
uint32_t nbslots = POK_CONFIG_SCHEDULING_NBSLOTS;
uint64_t pok_sched_slots[POK_CONFIG_SCHEDULING_NBSLOTS] =
    (uint64_t[])POK_CONFIG_SCHEDULING_SLOTS;
uint8_t pok_sched_slots_allocation[POK_CONFIG_SCHEDULING_NBSLOTS] =
    (uint8_t[])POK_CONFIG_SCHEDULING_SLOTS_ALLOCATION;
uint8_t pok_config_partition_priority[POK_CONFIG_SCHEDULING_NBSLOTS] =
    (uint8_t[])POK_CONFIG_PARTITIONS_PRIORITY;
uint8_t pok_config_partition_period[POK_CONFIG_SCHEDULING_NBSLOTS] =
    (uint8_t[])POK_CONFIG_PARTITIONS_PERIOD;
uint8_t pok_config_partition_deadline[POK_CONFIG_SCHEDULING_NBSLOTS] =
    (uint8_t[])POK_CONFIG_PARTITIONS_DEADLINE;
uint8_t pok_config_partition_weight[POK_CONFIG_SCHEDULING_NBSLOTS] =
    (uint8_t[])POK_CONFIG_PARTITIONS_WEIGHT;

uint64_t pok_sched_next_deadline;
uint64_t pok_sched_next_major_frame;
uint64_t pok_sched_next_flush; // variable used to handle user defined
                               // flushing period, i.e. distinct from
                               // MAF and from partition slot
                               // boundaries

uint8_t pok_sched_current_slot =
    0; /* Which slot are we executing at this time ?*/

extern int spinlocks[POK_CONFIG_NB_PROCESSORS];

void pok_sched_thread_switch(void);

/**
 *\\brief Init scheduling service
 */
static int32_t get_gcd_partition_weight(uint32_t index_low, uint32_t index_high)
{
   uint32_t i,j,k,m;
   int32_t gcd_weight = 0;

   gcd_weight = pok_config_partition_weight[index_low];
   if(gcd_weight <= 1)
      return 1;
 
   for (i = index_low + 1; i <= index_high; i++)
   {
          j = gcd_weight;
          k = pok_config_partition_weight[i];
          
          m = j%k;
          while(m != 0)
          {
            j = k;
            k = m;
            m = j % k;      
          }
          gcd_weight = k;
          if(gcd_weight <= 1)
             return 1;

   }

   return gcd_weight;
}

static int32_t get_max_partition_weight(uint32_t index_low, uint32_t index_high)
{
   uint32_t i;
   int32_t max_weight = 0;
   for (i = index_low; i <= index_high; i++)
   {
      if (pok_config_partition_weight[i] > max_weight )
         max_weight = pok_config_partition_weight[i];
   }
   //printf("%d\n", max_weight);
   return max_weight;
}

void pok_sched_init(void) {
  /*
   * We check that the total time of time frame
   * corresponds to the sum of each slot
   */
  uint64_t total_time;
  uint8_t slot;
  fence = barr = 0;

  total_time = 0;

  for (slot = 0; slot < POK_CONFIG_SCHEDULING_NBSLOTS; slot++) {
    total_time = total_time + pok_sched_slots[slot];
  }

  if (total_time != POK_CONFIG_SCHEDULING_MAJOR_FRAME) {
#ifdef POK_NEEDS_DEBUG
    printf("Major frame is not compliant with all time slots\n");
#endif
    pok_kernel_error(POK_ERROR_KIND_KERNEL_CONFIG);
  }

  uint8_t i = 0;
  uint8_t temp8 = 0;
  uint64_t temp64 = 0;
  uint32_t current_slot = 0;
   switch (POK_CONFIG_SCHEDULER_FOR_PARTITIONS){
      case POK_SCHED_RR:
         for (slot = 0; slot < nbslots; slot++)
         {
            pok_sched_slots_allocation[slot] =  current_slot;
            current_slot = (current_slot+1) % nbslots;
         }
         break;
      case POK_SCHED_PRIORITY:
        for (slot = 0; slot < nbslots; slot++)
        {
            pok_sched_slots_allocation[slot] =  current_slot;
            current_slot = (current_slot+1) % nbslots;
        }
        i = 0;
        temp8 = 0;
        temp64 = 0;
        for (slot = nbslots-1; slot > 0; slot--)
        {
          for (i =0; i < slot ; i++){
              if( pok_config_partition_priority[i] < pok_config_partition_priority[i+1] )
              {
                temp8 = pok_sched_slots_allocation[i];
                pok_sched_slots_allocation[i] =pok_sched_slots_allocation[i+1];
                pok_sched_slots_allocation[i+1] = temp8;
                temp8 = pok_config_partition_priority[i];
                pok_config_partition_priority[i] = pok_config_partition_priority[i+1];
                pok_config_partition_priority[i+1] = temp8;
                temp8 = pok_config_partition_period[i];
                pok_config_partition_period[i] = pok_config_partition_period[i+1];
                pok_config_partition_period[i+1] = temp8;
                temp8 = pok_config_partition_deadline[i];
                pok_config_partition_deadline[i] = pok_config_partition_deadline[i+1];
                pok_config_partition_deadline[i+1] = temp8;
                temp8 = pok_config_partition_weight[i];
                pok_config_partition_weight[i] = pok_config_partition_weight[i+1];
                pok_config_partition_weight[i+1] = temp8;
                temp64 = pok_sched_slots[i];
                pok_sched_slots[i] = pok_sched_slots[i+1];
                pok_sched_slots[i+1] = temp64;
              }
          }
        }
        break;

      case POK_SCHED_EDF:
        for (slot = 0; slot < nbslots; slot++)
        {
            pok_sched_slots_allocation[slot] =  current_slot;
            current_slot = (current_slot + 1) % nbslots;
        }
        i = 0;
        temp8 = 0;
        temp64 = 0;
        for (slot = nbslots-1; slot > 0; slot--)
        {
          for (i =0; i < slot ; i++){
              if( pok_config_partition_deadline[i] > pok_config_partition_deadline[i+1] )
              {
                temp8 = pok_sched_slots_allocation[i];
                pok_sched_slots_allocation[i] =pok_sched_slots_allocation[i+1];
                pok_sched_slots_allocation[i+1] = temp8;
                temp8 = pok_config_partition_priority[i];
                pok_config_partition_priority[i] = pok_config_partition_priority[i+1];
                pok_config_partition_priority[i+1] = temp8;
                temp8 = pok_config_partition_period[i];
                pok_config_partition_period[i] = pok_config_partition_period[i+1];
                pok_config_partition_period[i+1] = temp8;
                temp8 = pok_config_partition_deadline[i];
                pok_config_partition_deadline[i] = pok_config_partition_deadline[i+1];
                pok_config_partition_deadline[i+1] = temp8;
                temp8 = pok_config_partition_weight[i];
                pok_config_partition_weight[i] = pok_config_partition_weight[i+1];
                pok_config_partition_weight[i+1] = temp8;
                temp64 = pok_sched_slots[i];
                pok_sched_slots[i] = pok_sched_slots[i+1];
                pok_sched_slots[i+1] = temp64;
              }
          }
        }
        break;
      case POK_SCHED_WRR:
         for (slot = 0; slot < nbslots; slot++)
         {
            pok_sched_slots_allocation[slot] =  current_slot;
            current_slot = (current_slot+1) % nbslots;
         }
         if(current_weight <= 0)
          current_weight = get_max_partition_weight(0, nbslots - 1);
        //printf("current weight %d  gcd weight %d\n",current_weight,get_gcd_partition_weight(0, nbslots - 1));
        i = 0;
        do {
          
          if (i >= nbslots) {
            i = 0;
            current_weight = current_weight - get_gcd_partition_weight(0, nbslots - 1);
            if(current_weight <= 0)
              current_weight = get_max_partition_weight(0, nbslots - 1);
            
            //printf("%d\n",current_weight);
          }
          //printf(" weight %d\n",i,pok_config_partition_weight[i]);
          if((pok_config_partition_weight[i] >= current_weight)){                    
            break;
          }
          i++;
        } while (TRUE);
        pok_sched_current_slot = i;
        pok_sched_next_major_frame = POK_CONFIG_SCHEDULING_MAJOR_FRAME;
        pok_sched_next_deadline = pok_sched_slots[i];
        pok_sched_next_flush = 0;
        pok_current_partition = pok_sched_slots_allocation[i];  

         break;

      default:
         for (slot = 0; slot < nbslots; slot++)
         {
            pok_sched_slots_allocation[slot] =  current_slot;
            current_slot = (current_slot + 1) % nbslots;
         }
         break;         
   }



  if(POK_CONFIG_SCHEDULER_FOR_PARTITIONS !=POK_SCHED_WRR)
  {
    //printf("%d\n", pok_current_partition);
    pok_sched_current_slot = 0;
    pok_sched_next_major_frame = POK_CONFIG_SCHEDULING_MAJOR_FRAME;
    pok_sched_next_deadline = pok_sched_slots[0];
    pok_sched_next_flush = 0;
    pok_current_partition = pok_sched_slots_allocation[0];
  }

}

uint8_t pok_sched_get_priority_min(const pok_sched_t sched_type) {
  (void)sched_type;
  /* At this time, we only support one scheduler */
  return 0;
}

uint8_t pok_sched_get_priority_max(const pok_sched_t sched_type) {
  (void)sched_type;
  /* At this time, we only support one scheduler */
  return 255;
}

uint8_t pok_elect_partition() {
  uint8_t next_partition = POK_SCHED_CURRENT_PARTITION;
#if POK_CONFIG_NB_PARTITIONS > 1
  uint64_t now = POK_GETTICK();
  //printf("now %lld  %lld\n",now, pok_sched_next_deadline);

  if (pok_sched_next_deadline <= now) {
    /* Here, we change the partition */
#if defined(POK_NEEDS_PORTS_SAMPLING) || defined(POK_NEEDS_PORTS_QUEUEING)
#if defined(POK_FLUSH_PERIOD)
    // Flush periodically all partition ports
    // nb : Flush periodicity is a multiple of POK time base.
    if (pok_sched_next_flush <= now) {
      pok_sched_next_flush += POK_FLUSH_PERIOD;
      pok_port_flushall();
    }
#elif defined(POK_NEEDS_FLUSH_ON_WINDOWS)
    // Flush only the ports of the partition that just finished its slot
    if ((pok_sched_next_deadline <= now)) {
      pok_port_flush_partition(pok_current_partition);
    }
#else  // activate default flushing policy at each Major Frame beginning
    if (pok_sched_next_major_frame <= now) {
      pok_sched_next_major_frame =
          pok_sched_next_major_frame + POK_CONFIG_SCHEDULING_MAJOR_FRAME;
      pok_port_flushall();
    }
#endif /* defined POK_FLUSH_PERIOD || POK_NEEDS_FLUSH_ON_WINDOWS */
#endif /* defined (POK_NEEDS_PORTS....) */

    if(POK_CONFIG_SCHEDULER_FOR_PARTITIONS ==POK_SCHED_WRR)
    {
        if(current_weight <= 0)
          current_weight = get_max_partition_weight(0, nbslots - 1);

        uint8_t i = pok_sched_current_slot;
        do {
          i++;
          if (i >= nbslots) {
            i = 0;
            current_weight = current_weight - get_gcd_partition_weight(0, nbslots - 1);
            if(current_weight <= 0)
              current_weight = get_max_partition_weight(0, nbslots - 1);
            
            //printf("%d\n",current_weight);
          }
          //printf("partition %d weight %d\n",i,pok_config_partition_weight[i]);
          if((pok_config_partition_weight[i] >= current_weight)){           
            break;
          }
        } while (TRUE);
        pok_sched_current_slot = i;
        pok_sched_next_deadline =
          pok_sched_next_deadline + pok_sched_slots[pok_sched_current_slot];

    }
    else{

    pok_sched_current_slot =
        (pok_sched_current_slot + 1) % POK_CONFIG_SCHEDULING_NBSLOTS;
    pok_sched_next_deadline =
        pok_sched_next_deadline + pok_sched_slots[pok_sched_current_slot];
    }

    /*
        *  FIXME : current debug session about exceptions-handled
          printf ("Switch from partition %d to partition %d\n",
       pok_current_partition, pok_sched_current_slot); printf ("old current
       thread = %d\n", POK_SCHED_CURRENT_THREAD);

          printf ("new current thread = %d\n",
       CURRENT_THREAD(pok_partitions[pok_sched_current_slot])); printf ("new
       prev current thread = %d\n",
       pok_partitions[pok_sched_current_slot].prev_thread);
          */
    next_partition = pok_sched_slots_allocation[pok_sched_current_slot];
  }
#endif /* POK_CONFIG_NB_PARTITIONS > 1 */

  return next_partition;
}

uint32_t pok_elect_thread(uint8_t new_partition_id) {
  uint64_t now = POK_GETTICK();
  pok_partition_t *new_partition = &(pok_partitions[new_partition_id]);
  //printf("now : %lld\n",now);
  /*
   * We unlock all WAITING threads if the waiting time is passed
   */
  uint8_t i; /* i is used to browse the partition. We support
              * only 255 partitions are max, so, we use an uin8_t
              * type
              */
  pok_thread_t *thread;
  for (i = 0; i < new_partition->nthreads; i++) {
    thread = &(pok_threads[new_partition->thread_index_low + i]);

    if (thread->processor_affinity == pok_get_proc_id()) {

#if defined(POK_NEEDS_LOCKOBJECTS) || defined(POK_NEEDS_PORTS_QUEUEING) ||     \
    defined(POK_NEEDS_PORTS_SAMPLING)
      if ((thread->state == POK_STATE_WAITING) &&
          (thread->wakeup_time <= now)) {
        thread->state = POK_STATE_RUNNABLE;
      }
#endif

      if ((thread->state == POK_STATE_WAIT_NEXT_ACTIVATION) &&
          (thread->next_activation <= now)) {
        assert(thread->time_capacity);
        thread->state = POK_STATE_RUNNABLE;
        thread->is_use = 0;
        thread->remaining_time_capacity = thread->time_capacity;
        thread->next_activation = thread->next_activation + thread->period;
        //printf("thread active");
      }
    }
  }

  /*
   * We elect the thread to be executed.
   */
  uint32_t elected;
  //printf("%d\n",new_partition->mode);
  switch (new_partition->mode) {
  case POK_PARTITION_MODE_INIT_COLD:
  case POK_PARTITION_MODE_INIT_WARM:
    if (pok_get_proc_id() == new_partition->thread_main_proc) {
      if ((new_partition->thread_error != 0) &&
          (pok_threads[new_partition->thread_error].state !=
           POK_STATE_STOPPED)) {
        elected = new_partition->thread_error;
      } else {
        elected = new_partition->thread_main;
      }

      elected = new_partition->thread_main;
    } else
      elected = IDLE_THREAD;
    break;

  case POK_PARTITION_MODE_NORMAL:
    if ((CURRENT_THREAD(*new_partition) == new_partition->thread_error) &&
        (pok_threads[CURRENT_THREAD(*new_partition)].state ==
         POK_STATE_RUNNABLE)) {
      elected = new_partition->thread_error;
      break;
    }
    
    if ((POK_SCHED_CURRENT_THREAD != IDLE_THREAD) &&
        (POK_SCHED_CURRENT_THREAD != POK_CURRENT_PARTITION.thread_main) &&
        (POK_SCHED_CURRENT_THREAD != POK_CURRENT_PARTITION.thread_error)) {
      //printf("remaining time: %lld \n",POK_CURRENT_THREAD.remaining_time_capacity);
      if (POK_CURRENT_THREAD.remaining_time_capacity > 0) {
        POK_CURRENT_THREAD.remaining_time_capacity =
            POK_CURRENT_THREAD.remaining_time_capacity - 1;
            //printf("    partition %d, thread %d, remaining time capacity is %lld\n",new_partition_id,POK_SCHED_CURRENT_THREAD,POK_CURRENT_THREAD.remaining_time_capacity);
            
        if (pok_partitions[POK_CURRENT_THREAD.partition].sched == POK_SCHED_NEWRR)
            {
               POK_CURRENT_THREAD.remaining_time_slices = 
                POK_CURRENT_THREAD.remaining_time_slices - 1;
            }
        if (POK_CURRENT_THREAD.remaining_time_capacity == 0 && POK_CURRENT_THREAD.time_capacity > 0)
               POK_CURRENT_THREAD.state = POK_STATE_WAIT_NEXT_ACTIVATION;
      } else if (POK_CURRENT_THREAD.time_capacity >
                 0) // Wait next activation only for thread
                    // with non-infinite capacity (could be
                    // infinite with value -1 <--> INFINITE_TIME_CAPACITY)
      {
        POK_CURRENT_THREAD.state = POK_STATE_WAIT_NEXT_ACTIVATION;
      }
    }
    elected = new_partition->sched_func(
        new_partition->thread_index_low, new_partition->thread_index_high,
        PREV_THREAD(*new_partition), CURRENT_THREAD(*new_partition));
#ifdef POK_NEEDS_INSTRUMENTATION
    if ((elected != IDLE_THREAD) && (elected != new_partition->thread_main)) {
      pok_instrumentation_running_task(elected);
    }
#endif

    break;

  default:
    elected = IDLE_THREAD;
    break;
  }

  // computed next thread's deadline
  if (pok_threads[POK_SCHED_CURRENT_THREAD].time_capacity > 0)
    pok_threads[elected].end_time =
        now + pok_threads[elected].remaining_time_capacity;
  //printf("state: %d\n",pok_threads[POK_SCHED_CURRENT_THREAD].state);
  return elected;
}

// Global scheduling with partition

uint8_t new_partition;

void pok_global_sched_thread(bool_t is_source_processor) {
  uint8_t elected_thread = pok_elect_thread(POK_SCHED_CURRENT_PARTITION);
  //printf("elect : %d\n",elected_thread);
  if (CURRENT_THREAD(pok_partitions[POK_SCHED_CURRENT_PARTITION]) !=
      elected_thread) {
    if (CURRENT_THREAD(pok_partitions[POK_SCHED_CURRENT_PARTITION]) !=
        IDLE_THREAD) {
      PREV_THREAD(pok_partitions[POK_SCHED_CURRENT_PARTITION]) =
          CURRENT_THREAD(pok_partitions[POK_SCHED_CURRENT_PARTITION]);
    }
    CURRENT_THREAD(pok_partitions[POK_SCHED_CURRENT_PARTITION]) =
        elected_thread;
  }
  pok_global_sched_context_switch(elected_thread, is_source_processor);
}
void pok_global_sched() {
  uint8_t elected_partition = POK_SCHED_CURRENT_PARTITION;
  elected_partition = pok_elect_partition();
  new_partition = elected_partition != POK_SCHED_CURRENT_PARTITION;
  POK_SCHED_CURRENT_PARTITION = elected_partition;

  if (multiprocessing_system) {
    start_rendezvous(&fence);
    pok_send_global_schedule_thread();
  }
  pok_global_sched_thread(TRUE);
  //printf("sched\n");
}

void synchro_processors(const uint32_t elected_id, bool_t is_source_processor) {
  if (!is_source_processor) {
    join_rendezvous(&fence);
    join_rendezvous(&barr);
  } else {
    if (multiprocessing_system) {
      spin_wait_for_rendezvous(&fence, multiprocessing_system);
      if (new_partition) {
        pok_space_switch(POK_CURRENT_THREAD.partition,
                         pok_threads[elected_id].partition);
      }
      start_rendezvous(&barr);
      unblock_rendezvous(&fence);
      spin_wait_for_rendezvous(&barr, multiprocessing_system);
      unblock_rendezvous(&barr);
    } else if (new_partition) {
      pok_space_switch(POK_CURRENT_THREAD.partition,
                       pok_threads[elected_id].partition);
    }
  }
}

/*
 * Context-switch function to switch from one thread to another
 * Rely on architecture-dependent functionnalities (must include arch.h)
 */
void pok_global_sched_context_switch(const uint32_t elected_id,
                                     bool_t is_source_processor) {
  uint32_t *current_sp;
  uint32_t new_sp;

  if (POK_SCHED_CURRENT_THREAD == elected_id) {
    synchro_processors(elected_id, is_source_processor);

    if (!is_source_processor)
      pok_end_ipi();

  } else {

#ifdef POK_NEEDS_LOCKOBJECTS

    // Check if every spin lock is unlocked before changing context
    assert(!spinlocks[pok_get_proc_id()]);

#endif

    current_sp = &POK_CURRENT_THREAD.sp;
    new_sp = pok_threads[elected_id].sp;

    synchro_processors(elected_id, is_source_processor);

    POK_SCHED_CURRENT_THREAD = elected_id;
    if (!is_source_processor)
      pok_end_ipi();
    pok_context_switch(current_sp, new_sp);
  }
}

// Local thread sched

void pok_sched_context_switch(const uint32_t elected_id,
                              bool_t is_source_processor) {
  uint32_t *current_sp;
  uint32_t new_sp;

  if (POK_SCHED_CURRENT_THREAD == elected_id) {
    if (!is_source_processor)
      pok_end_ipi();

  } else {

#ifdef POK_NEEDS_LOCKOBJECTS
    // Check if every spin lock is unlocked before changing context
    assert(!spinlocks[pok_get_proc_id()]);
#endif

    current_sp = &POK_CURRENT_THREAD.sp;
    new_sp = pok_threads[elected_id].sp;

    POK_SCHED_CURRENT_THREAD = elected_id;
    if (!is_source_processor)
      pok_end_ipi();
    pok_context_switch(current_sp, new_sp);
  }
}

void pok_sched_thread(bool_t is_source_processor) {
  uint8_t elected_thread = pok_elect_thread(POK_SCHED_CURRENT_PARTITION);
  //printf("elect2 : %d\n",elected_thread);
  if (CURRENT_THREAD(pok_partitions[POK_SCHED_CURRENT_PARTITION]) !=
      elected_thread) {
    if (CURRENT_THREAD(pok_partitions[POK_SCHED_CURRENT_PARTITION]) !=
        IDLE_THREAD) {
      PREV_THREAD(pok_partitions[POK_SCHED_CURRENT_PARTITION]) =
          CURRENT_THREAD(pok_partitions[POK_SCHED_CURRENT_PARTITION]);
    }
    CURRENT_THREAD(pok_partitions[POK_SCHED_CURRENT_PARTITION]) =
        elected_thread;
  }
  pok_sched_context_switch(elected_thread, is_source_processor);
}

// Send local thread sched

void pok_threads_schedule_one_proc(uint8_t dest) {
  if (multiprocessing_system) {
    if (dest == pok_get_proc_id())
      pok_sched_thread(TRUE);
    else
      pok_send_schedule_thread(dest);
  } else {
    assert(!dest);
    pok_sched_thread(TRUE);
  }
}

void pok_threads_schedule_every_proc() {
  if (multiprocessing_system) {
    pok_send_schedule_thread_other_processors();
  }
  pok_sched_thread(TRUE);
}

#ifdef POK_NEEDS_SCHED_RMS
uint32_t pok_sched_part_rms(const uint32_t index_low, const uint32_t index_high,
                            const uint32_t __attribute__((unused)) prev_thread,
                            const uint32_t __attribute__((unused))
                            current_thread) {
  uint32_t res;
  uint8_t current_proc = pok_get_proc_id();
#ifdef POK_NEEDS_DEBUG
  uint32_t from;
  from = prev_thread;
#endif

  res = index_low;

  do {
    res++;
    if (res >= index_high) {
      res = index_low;
    }
  } while ((res != index_low) &&
           ((pok_threads[res].state != POK_STATE_RUNNABLE) ||
            (pok_threads[res].processor_affinity != current_proc)));

  if ((res == index_low) &&
      ((pok_threads[res].state != POK_STATE_RUNNABLE) ||
       (pok_threads[res].processor_affinity != current_proc))) {
    res = IDLE_THREAD;
  }

#ifdef POK_NEEDS_DEBUG
  if (res != IDLE_THREAD || current_thread != IDLE_THREAD) {
    if (res == IDLE_THREAD) {
      printf("--- Scheduling processor: %hhd\n    scheduling idle thread\n\t\t",
             current_proc);
    } else {
      printf("--- Scheduling processor: %hhd\n    scheduling thread: %d {%lld} "
             "--- ",
             current_proc, res, pok_threads[res].period);
      from = index_low;
      while (from <= index_high) {
        if (pok_threads[from].state == POK_STATE_RUNNABLE &&
            pok_threads[from].processor_affinity == current_proc) {
          printf(" %d {%lld} ,", from, pok_threads[from].period);
        }
        from++;
      }
      printf(" are runnable; \n\t\t");
    }
    from = index_low;
    while (from <= index_high) {
      if (pok_threads[from].state != POK_STATE_RUNNABLE &&
          pok_threads[from].processor_affinity == current_proc) {
        printf(" %d (%s)", from, state_names[pok_threads[from].state]);
      }
      from++;
    }
    printf(" are NOT runnable;\n");
  }
#endif

  return res;
}
#endif /* POK_NEEDS_SCHED_RMS */

#ifdef POK_NEEDS_SCHED_STATIC
uint32_t pok_sched_part_static(const uint32_t index_low,
                               const uint32_t index_high,
                               const uint32_t prev_thread,
                               const uint32_t current_thread) {
  uint32_t from = current_thread != IDLE_THREAD ? current_thread : prev_thread;
  int32_t max_prio = -1;
  uint32_t max_thread = current_thread;
  uint8_t current_proc = pok_get_proc_id();

  if (prev_thread == IDLE_THREAD)
    from = index_low;

  uint32_t i = from;
  do {
    if (pok_threads[i].state == POK_STATE_RUNNABLE &&
        pok_threads[i].processor_affinity == current_proc &&
        pok_threads[i].priority > max_prio) {
      max_prio = pok_threads[i].priority;
      max_thread = i;
    }
    i++;
    if (i >= index_high) {
      i = index_low;
    }
  } while (i != from);

  uint32_t elected = max_prio >= 0 ? max_thread : IDLE_THREAD;

#ifdef POK_NEEDS_DEBUG
  if (elected != current_thread &&
      (elected != IDLE_THREAD || current_thread != IDLE_THREAD)) {
    uint32_t non_ready = 0;
    if (elected == IDLE_THREAD) {
      printf("--- Scheduling processor: %hhd\n    scheduling idle thread\n",
             current_proc);
      non_ready = index_high - index_low;
    } else {
      uint32_t first = 1;
      printf("--- Scheduling processor: %hhd\n    scheduling thread %d "
             "(priority "
             "%d) (period %lld) (time capacity %lld)\n",
             current_proc, elected, pok_threads[elected].priority, pok_threads[elected].period, pok_threads[elected].time_capacity);
      for (uint32_t i = index_low; i < index_high; i++) {
        if (pok_threads[i].state == POK_STATE_RUNNABLE &&
            pok_threads[i].processor_affinity == current_proc) {
          if (i != elected) {
            printf("%s %d (%d)", first ? "    other ready: " : ",", i,
                   pok_threads[i].priority);
            first = 0;
          }
        } else {
          non_ready++;
        }
      }
      if (!first) {
        printf("\n");
      }
    }
    if (non_ready) {
      printf("    non-ready:");
      uint32_t first = 1;
      for (uint32_t i = index_low; i < index_high; i++) {
        if (pok_threads[i].state != POK_STATE_RUNNABLE &&
            pok_threads[i].processor_affinity == current_proc) {
          printf("%s %d (%d/%s)", first ? "" : ",", i, pok_threads[i].priority,
                 state_names[pok_threads[i].state]);
          first = 0;
        }
      }
      printf("\n");
    }
  }
#endif
  return elected;
}
#endif // POK_NEEDS_SCHED_STATIC

uint32_t pok_sched_part_rr(const uint32_t index_low, const uint32_t index_high,
                           const uint32_t prev_thread,
                           const uint32_t current_thread) {
  uint32_t elected;
  uint32_t from;
  uint8_t current_proc = pok_get_proc_id();

  if (current_thread == IDLE_THREAD) {
    elected = (prev_thread != IDLE_THREAD) ? prev_thread : index_low;
  } else {
    elected = current_thread;
  }

  from = elected;

  if ((pok_threads[current_thread].remaining_time_capacity > 0 ||
       pok_threads[current_thread].time_capacity == INFINITE_TIME_VALUE) &&
      (pok_threads[current_thread].state == POK_STATE_RUNNABLE) &&
      (pok_threads[current_thread].processor_affinity == current_proc) &&
      current_thread != IDLE_THREAD) {
    return current_thread;
  }

  do {
    elected++;
    if (elected >= index_high) {
      elected = index_low;
    }
  } while ((elected != from) &&
           ((pok_threads[elected].state != POK_STATE_RUNNABLE) ||
            (pok_threads[elected].processor_affinity != current_proc)));

  if ((elected == from) &&
      ((pok_threads[elected].state != POK_STATE_RUNNABLE) ||
       (pok_threads[elected].processor_affinity != current_proc))) {
    elected = IDLE_THREAD;
  }

#ifdef POK_NEEDS_DEBUG
  if (elected != current_thread &&
      (elected != IDLE_THREAD || current_thread != IDLE_THREAD)) {
    printf("--- scheduling partition: %d, low:%d, high:%d\n",
           pok_current_partition, index_low, index_high);
    uint32_t non_ready = 0;
    if (elected == IDLE_THREAD) {
      printf("--- Scheduling processor: %hhd\n    scheduling idle thread\n",
             current_proc);
      non_ready = index_high - index_low;
    } else {
      uint32_t first = 1;
      printf("--- Scheduling processor: %hhd\n    scheduling thread %d "
             "(priority "
             "%d)\n",
             current_proc, elected, pok_threads[elected].priority);
      for (uint32_t i = index_low; i < index_high; i++) {
        if (pok_threads[i].state == POK_STATE_RUNNABLE &&
            pok_threads[i].processor_affinity == current_proc) {
          if (i != elected) {
            printf("%s %d (%d)", first ? "    other ready: " : ",", i,
                   pok_threads[i].priority);
            first = 0;
          } else
            printf("elected %d !!! \n", elected);
        } else {
          non_ready++;
        }
      }
      if (!first) {
        printf("\n");
      }
    }
    if (non_ready) {
      printf("    non-ready:");
      uint32_t first = 1;
      for (uint32_t i = index_low; i < index_high; i++) {
        if (pok_threads[i].state != POK_STATE_RUNNABLE &&
            pok_threads[i].processor_affinity == current_proc) {
          printf("%s %d (%d/%s)", first ? "" : ",", i, pok_threads[i].priority,
                 state_names[pok_threads[i].state]);
          first = 0;
        }
      }
      printf("\n");
    }
  }
#endif
  return elected;
}


/*our scheduler*/
uint32_t pok_sched_part_edf(const uint32_t index_low, const uint32_t index_high,
                           const uint32_t prev_thread,
                           const uint32_t current_thread){
  uint32_t from = current_thread != IDLE_THREAD ? current_thread : prev_thread;
  from = index_low;
  uint64_t min_ddl = 0xFFFFFFFFFFFFFFFF;
  uint32_t max_thread = current_thread;
  uint8_t current_proc = pok_get_proc_id();

  //if (prev_thread == IDLE_THREAD)
  from = index_low;

  uint32_t i = from;
  do {
    if (pok_threads[i].state == POK_STATE_RUNNABLE &&
        pok_threads[i].processor_affinity == current_proc &&
        pok_threads[i].deadline < min_ddl) {
      min_ddl = pok_threads[i].deadline;
      max_thread = i;
    }
    i++;
    if (i >= index_high) {
      i = index_low;
    }
  } while (i != from);

  uint32_t elected = min_ddl < 0xFFFFFFFFFFFFFFFF ? max_thread : IDLE_THREAD;

  return elected;

}

uint32_t pok_sched_part_priority(const uint32_t index_low, const uint32_t index_high,
                           const uint32_t prev_thread,
                           const uint32_t current_thread){
  uint32_t from = current_thread != IDLE_THREAD ? current_thread : prev_thread;
  int32_t max_prio = -1;
  uint32_t max_thread = current_thread;
  uint8_t current_proc = pok_get_proc_id();

  //if (prev_thread == IDLE_THREAD)
  from = index_low;

  uint32_t i = from;
  do {
    if (pok_threads[i].state == POK_STATE_RUNNABLE &&
        pok_threads[i].processor_affinity == current_proc &&
        pok_threads[i].priority > max_prio) {
      max_prio = pok_threads[i].priority;
      max_thread = i;
    }
    i++;
    if (i >= index_high) {
      i = index_low;
    }
  } while (i != from);

  uint32_t elected = max_prio >= 0 ? max_thread : IDLE_THREAD;

  return elected;

}

uint32_t pok_sched_part_newrr(const uint32_t index_low, const uint32_t index_high,
                           const uint32_t prev_thread,
                           const uint32_t current_thread){
  uint32_t from = current_thread != IDLE_THREAD ? current_thread : prev_thread;

  if (prev_thread == IDLE_THREAD)
    from = index_low;  

  if ((pok_threads[current_thread].remaining_time_capacity > 0) && 
      (pok_threads[current_thread].remaining_time_slices > 0) && 
      (pok_threads[current_thread].state == POK_STATE_RUNNABLE)){
      return current_thread;
   }

  uint32_t i = from;
  do {
    i++;
    if (i >= index_high) {
      i = index_low;
    }
    if((pok_threads[i].state == POK_STATE_RUNNABLE) &&
        (pok_threads[i].remaining_time_capacity > 0)){
      
      break;
    }
    
  } while (i != from);
  uint32_t elected = i;
  if (elected != from)
   {
      if(pok_threads[elected].remaining_time_capacity > TIME_SLICES)
        pok_threads[elected].remaining_time_slices = TIME_SLICES;
      else pok_threads[elected].remaining_time_slices = pok_threads[elected].remaining_time_capacity;
   }

   if ((elected == from) && pok_threads[elected].state != POK_STATE_RUNNABLE)
   {
      elected = IDLE_THREAD;
   }

  return elected;
                          
}

static int32_t get_gcd_weight(uint32_t index_low, uint32_t index_high)
{
   uint32_t i,j,k,m;
   int32_t gcd_weight = 0;
   for (i = index_low + 1; i <= index_high; i++)
     if (pok_threads[i].state == POK_STATE_RUNNABLE)
     {
       gcd_weight = pok_threads[i].weight;
       if(gcd_weight <= 1)
             return 1;
       break;
     } 
   if(i > index_high)
     return 0;  
   for (i = i + 1; i <= index_high; i++)
   {
      if (pok_threads[i].state == POK_STATE_RUNNABLE)
      {
          j = gcd_weight;
          k = pok_threads[i].weight;
          
          m = j%k;
          while(m != 0)
          {
            j = k;
            k = m;
            m = j % k;      
          }
          gcd_weight = k;
          if(gcd_weight <= 1)
             return 1;
      }
      
   }

   return gcd_weight;
}

static int32_t get_max_weight(uint32_t index_low, uint32_t index_high)
{
   uint32_t i;
   int32_t max_weight = 0;
   for (i = index_low; i <= index_high; i++)
   {
      //printf("id %d weight %d state %d\n", i,pok_threads[i].weight,pok_threads[i].state);
      if (pok_threads[i].weight > max_weight && 
            pok_threads[i].state == POK_STATE_RUNNABLE )
         max_weight = pok_threads[i].weight;
   }
   
   return max_weight;
}

uint32_t pok_sched_part_wrr(const uint32_t index_low, const uint32_t index_high,
                           const uint32_t prev_thread,
                           const uint32_t current_thread){
  //int32_t gcd_weight = get_gcd_weight(index_low + 1, index_high - 1);
  //printf("low %d high %d gcd %d ",index_low, index_high, gcd_weight);
  //int32_t max_weight = get_max_weight(index_low + 1, index_high - 1);
  //printf("max %d\n",max_weight);
  uint32_t from = current_thread != IDLE_THREAD ? current_thread : prev_thread;
  
  uint32_t elected = IDLE_THREAD;
  if(get_max_weight(index_low + 1, index_high - 1) == 0)
     return elected;
  if(pok_partitions[pok_current_partition].current_weight <= 0)
    pok_partitions[pok_current_partition].current_weight = get_max_weight(index_low + 1, index_high - 1);
  if (prev_thread == IDLE_THREAD)
    from = index_low;  
 
  uint32_t i = from;
  do {
    i++;
    if (i >= index_high) {
      i = index_low;
      pok_partitions[pok_current_partition].current_weight = 
        pok_partitions[pok_current_partition].current_weight - get_gcd_weight(index_low + 1, index_high - 1);
      if(pok_partitions[pok_current_partition].current_weight <= 0)
        pok_partitions[pok_current_partition].current_weight = get_max_weight(index_low + 1, index_high - 1);
      if(pok_partitions[pok_current_partition].current_weight == 0)
        break;
      //printf("%d\n",pok_partitions[pok_current_partition].current_weight);
    }
    if((pok_threads[i].state == POK_STATE_RUNNABLE) &&
        (pok_threads[i].weight >= pok_partitions[pok_current_partition].current_weight)){
      
      break;
    }
  } while (TRUE);
  
  elected = i;
  if(pok_threads[elected].state != POK_STATE_RUNNABLE)
    elected = IDLE_THREAD;
  return elected;
}

uint32_t pok_sched_part_mrc(const uint32_t index_low, const uint32_t index_high,
                           const uint32_t prev_thread,
                           const uint32_t current_thread){
  uint32_t from = current_thread != IDLE_THREAD ? current_thread : prev_thread;
  from = index_low;
  uint64_t min_ddl = 0xFFFFFFFFFFFFFFFF;
  uint32_t max_thread = current_thread;
  uint8_t current_proc = pok_get_proc_id();
  int use_capacity=0;
  uint32_t i = from;
  do {
    use_capacity = pok_threads[i].space_capacity;
    if(use_capacity > 100){
      use_capacity = 100 - use_capacity;     
    }
    if (pok_threads[i].state == POK_STATE_RUNNABLE &&
        pok_threads[i].processor_affinity == current_proc &&
        pok_threads[i].deadline < min_ddl ) {
      if(pok_threads[i].is_use == 1 || (use_capacity + pok_partitions[pok_current_partition].current_space_capacity >=0 && 
         use_capacity + pok_partitions[pok_current_partition].current_space_capacity <= SPACE_CAPACITY ))
      {
          min_ddl = pok_threads[i].deadline;
          max_thread = i;          
      }     
    }
    i++;
    if (i >= index_high) {
      i = index_low;
    }
  } while (i != from);
 
  uint32_t elected = min_ddl < 0xFFFFFFFFFFFFFFFF ? max_thread : IDLE_THREAD;
  if(elected != IDLE_THREAD && pok_threads[elected].is_use == 0 && pok_threads[elected].remaining_time_capacity == 1){
    pok_threads[elected].is_use = 1;
    use_capacity = pok_threads[elected].space_capacity;
    if(use_capacity > 100){
      use_capacity = 100 - use_capacity;     
    }
    pok_partitions[pok_current_partition].current_space_capacity += use_capacity;
  }
   //printf("%d\n",pok_partitions[pok_current_partition].current_space_capacity);  

  return elected;
}


#if defined(POK_NEEDS_LOCKOBJECTS) || defined(POK_NEEDS_PORTS_QUEUEING) ||     \
    defined(POK_NEEDS_PORTS_SAMPLING)
void pok_sched_unlock_thread(const uint32_t thread_id) {
  pok_threads[thread_id].state = POK_STATE_RUNNABLE;
}
#endif

#if defined(POK_NEEDS_LOCKOBJECTS) || defined(POK_NEEDS_PORTS_QUEUEING) ||     \
    defined(POK_NEEDS_PORTS_SAMPLING) || defined(POK_NEEDS_THREAD_SLEEP) ||    \
    defined(POK_NEEDS_THREAD_SLEEP_UNTIL)
void pok_sched_lock_current_thread(void) {
  pok_threads[POK_SCHED_CURRENT_THREAD].state = POK_STATE_LOCK;
}

void pok_sched_lock_current_thread_timed(const uint64_t time) {
  pok_threads[POK_SCHED_CURRENT_THREAD].state = POK_STATE_WAITING;
  pok_threads[POK_SCHED_CURRENT_THREAD].wakeup_time = time;
}
#endif

void pok_sched_stop_self(void) {
  POK_CURRENT_THREAD.state = POK_STATE_STOPPED;
  pok_sched_thread(TRUE);
}

void pok_sched_stop_thread(const uint32_t tid) {
  pok_threads[tid].state = POK_STATE_STOPPED;
}

#ifdef POK_NEEDS_DEPRECIATED
void pok_sched_lock_thread(const uint32_t thread_id) {
  pok_threads[thread_id].state = POK_STATE_LOCK;
}
#endif

pok_ret_t pok_sched_end_period() {
  POK_CURRENT_THREAD.state = POK_STATE_WAIT_NEXT_ACTIVATION;
  POK_CURRENT_THREAD.remaining_time_capacity = 0;
  pok_sched_thread(TRUE);
  return POK_ERRNO_OK;
}

void pok_sched_activate_error_thread(void) {
  uint32_t error_thread = pok_partitions[pok_current_partition].thread_error;
  if (error_thread != 0) {
    pok_threads[error_thread].priority = pok_sched_get_priority_max(0);
    pok_threads[error_thread].remaining_time_capacity = 1000;
    pok_threads[error_thread].period = 100;
    pok_threads[error_thread].next_activation = 0;

    pok_threads[error_thread].state = POK_STATE_RUNNABLE;
    pok_sched_context_switch(error_thread, TRUE);
  }
}

uint32_t pok_sched_get_current(uint32_t *thread_id) {
  if (pok_partitions[pok_current_partition].thread_error == 0)
    return POK_ERRNO_THREAD;
  if (KERNEL_THREAD == POK_SCHED_CURRENT_THREAD ||
      IDLE_THREAD == POK_SCHED_CURRENT_THREAD) {
    return POK_ERRNO_THREAD;
  }
  *thread_id = POK_SCHED_CURRENT_THREAD;
  return POK_ERRNO_OK;
}
