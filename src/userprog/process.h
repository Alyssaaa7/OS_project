#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

typedef struct args_addr {
  void* addr;
  struct list_elem elem;
} args_addr;

typedef struct list args_addr_list_t;

tid_t process_execute(const char* file_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(void);

#endif /* userprog/process.h */
