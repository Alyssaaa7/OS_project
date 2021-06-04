#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stddef.h>

void syscall_init(void);
void check_ptr(void* ptr, size_t size);
static void syscall_handler(struct intr_frame*);

/* Helper function. */
void terminate(int exit_code);

#endif /* userprog/syscall.h */
