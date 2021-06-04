#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include "threads/fixed-point.h"

/* States in a thread's life cycle. */
enum thread_status {
  THREAD_RUNNING, /* Running thread. */
  THREAD_READY,   /* Not running but ready to run. */
  THREAD_BLOCKED, /* Waiting for an event to trigger. */
  THREAD_DYING    /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t)-1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0      /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63     /* Highest priority. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread {
  /* Owned by thread.c. */
  tid_t tid;                 /* Thread identifier. */
  enum thread_status status; /* Thread state. */
  char name[16];             /* Name (for debugging purposes). */
  uint8_t* stack;            /* Saved stack pointer. */
  int priority;              /* Priority. */
  struct list_elem allelem;  /* List element for all threads list. */

#ifdef USERPROG
  struct wait_info* wait_info;    /* wait_info struct for current thread. */
  struct list children_wait_info; /* list of wait_info for children threads. */
  struct list fdt;                /* Fd table, consist of a list of table entries. */
  int next_aval_fd;               /* Keeps track of the next available fd to assign. */
  struct file* cur_file;          /* record current file */
#endif

  /* ================Project 2================ */
  int64_t wakeup_tick;    /* Time to wake up. */
  int effective_priority; /* The effective priority of the thread after donation */
  struct thread* donee;   /* Pointer to the thread that current thread is waiting on.*/
  struct list donors;     /* threads donating to this thread */

  struct list_elem donor_elem; /* List element for donors list. */
  struct list_elem sleepelem;  /* List element for sleep_list. */
  /* ========================================= */

  /* Shared between thread.c and synch.c. */
  struct list_elem elem; /* List element. */

#ifdef USERPROG
  /* Owned by userprog/process.c. */
  uint32_t* pagedir; /* Page directory. */
  /* ================Project 3================ */
  struct dir* cwd; /* CWD of the process. */
  /* ========================================= */
#endif

  /* Owned by thread.c. */
  unsigned magic; /* Detects stack overflow. */
};

/* wait_info is shared between a parent and a child thread. */
struct wait_info {
  struct semaphore sema; // Semaphore shared between the parent and child thread.
  char* file_name;       // File name of the program, used in loading the executable. */
  int ref_cnt; // keeps track of how many threads have reference to this struct, free memory when it is 0.
  struct lock lock;      // allows mutual exclusion when modifying members in the struct
  int exit_code;         // Pointer to the child’s exit_code.
  bool loaded;           // whether the child process have successfully loaded executable
  bool waited;           // whether the parent is has waited for the child
  tid_t tid;             // tid of the child thread
  struct list_elem elem; // for creating a list of children_wait_info
};

/* A row in our fd table. */
struct fd_row {
  int fd;
  struct list_elem elem;
  struct file* open_file_object;
  bool is_dir;
  void* dir_or_file;
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void* aux);
tid_t thread_create(const char* name, int priority, thread_func*, void*);

void thread_block(void);
void thread_unblock(struct thread*);

struct thread* thread_current(void);
tid_t thread_tid(void);
const char* thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func(struct thread* t, void* aux);
void thread_foreach(thread_action_func*, void*);

int thread_get_priority(void);
void thread_set_priority(int);

/* Project 2 */
void check_thread_yield(void);
bool priority_comp(const struct list_elem* elem1, const struct list_elem* elem2, void* aux);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

#endif /* threads/thread.h */
