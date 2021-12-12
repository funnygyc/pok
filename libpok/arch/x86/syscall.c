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

#include <core/syscall.h>
#include <core/thread.h>
#include <libc/stdio.h>
#include <types.h>

pok_ret_t pok_do_syscall(pok_syscall_id_t syscall_id,
                         pok_syscall_args_t *args) {
  pok_ret_t ret;
  uint32_t args_addr;
  uint32_t id;

  args_addr = (uint32_t)args;
  id = (uint32_t)syscall_id;

  //pok_thread_attr_t *attr = (pok_thread_attr_t *)(args->arg2);
  //if (args->nargs == 2)
  //  printf("weight point2: %d\n", (int)args->arg2);

  asm volatile("movl %1,%%eax \n\t"
               "movl %2,%%ebx \n\t"
               "int  $42 \n\t"
               "movl %%eax, %0"
               : "=g"(ret)
               : "g"(id), "g"(args_addr)
               : "%eax", "%ebx");
  return ret;
}
