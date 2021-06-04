#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"

static void syscall_handler(struct intr_frame*);
<<<<<<< HEAD

=======
>>>>>>> 9a3485af536ad5468069ee127682332d7be91969

/*  check whether the pointer is valid. */
void check_ptr(void* ptr, size_t size) {
  struct thread* curr_thread = thread_current();
  if (!ptr) {
    terminate(-1);
  } else if (!is_user_vaddr(ptr)) {
    terminate(-1);
  } else if (!is_user_vaddr(ptr + size)) {
    terminate(-1);
  } else if (!pagedir_get_page(curr_thread->pagedir, ptr)) {
    terminate(-1);
  } else if (!pagedir_get_page(curr_thread->pagedir, ptr + size)) {
    terminate(-1);
  }
}

<<<<<<< HEAD
void syscall_init(void) {

  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}
=======
void syscall_init(void) { intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall"); }
>>>>>>> 9a3485af536ad5468069ee127682332d7be91969

static void syscall_handler(struct intr_frame* f UNUSED) {
  uint32_t* args = ((uint32_t*)f->esp);

  /*
   * The following print statement, if uncommented, will print out the syscall
   * number whenever a process enters a system call. You might find it useful
   * when debugging. It will cause tests to fail, however, so you should not
   * include it in your final submission.
   */

  check_ptr(args, sizeof(uint32_t));
  check_ptr(&args[1], sizeof(uint32_t));


  switch (args[0]) {
    case SYS_PRACTICE:
      f->eax = args[1] + 1;
      break;

    case SYS_EXIT:
      f->eax = args[1];
      terminate(args[1]);
      break;

    case SYS_EXEC: {
      check_ptr((void*)args[1], sizeof(char*));
      tid_t tid = process_execute((char*)args[1]);
      if (tid == TID_ERROR) {
        f->eax = -1;
      } else {
        f->eax = tid;
      }
    } break;

    case SYS_HALT:
      shutdown_power_off();
      break;

    case SYS_WAIT: {
      tid_t tid = (tid_t)args[1];
      f->eax = process_wait(tid);
    } break;

    case SYS_CREATE: {
      check_ptr((void*)args[1], sizeof(char*));
      char* file_name = (char*)args[1];
      f->eax = filesys_create(file_name, args[2], false);
    } break;

    case SYS_REMOVE: {
      check_ptr((void*)args[1], sizeof(char*));
      char* file_name = (char*)args[1];
      char* last_part = malloc(NAME_MAX + 1);
      struct inode* inode;
      struct dir* last_dir = get_dir_path(file_name, last_part);
      bool success;
      int a ;
      int b;

      if (last_dir != NULL && dir_lookup(last_dir, last_part, &inode) && inode_is_dir(inode)) {
        a = last_dir->file_dir_count;
        b = dir_open(inode)->file_dir_count;
        dir_readdir(last_dir,last_part);

        if (dir_open(inode)->file_dir_count != 0) {
          f->eax = false;

        } else {
          success = dir_remove(last_dir, last_part);

          if (success) {
            last_dir->file_dir_count--;
          }
          f->eax = success;
        }
      } else {
        success = filesys_remove(last_part, last_dir);
        if (success) {
          last_dir->file_dir_count--;
        }
        f->eax = success;
      }
      free(last_part);
      inode_close(inode);
    } break;

    case SYS_OPEN: {
      check_ptr((void*)args[1], sizeof(char*));

      char* file_name = (char*)args[1];
      char* last_part = malloc(NAME_MAX + 1);
      struct thread* current_thread = thread_current();
      struct dir* dir = get_dir_path(file_name, last_part);
      struct inode* inode;

      if (strcmp(file_name, "/") == 0) {
        struct dir* new_dir = dir_open_root();
        struct fd_row* new_fd = malloc(sizeof(struct fd_row));
        new_fd->fd = current_thread->next_aval_fd++;
        new_fd->open_file_object = NULL;
        new_fd->is_dir = true;
        new_fd->dir_or_file = (void*)new_dir;
        list_push_back(&current_thread->fdt, &new_fd->elem);

        f->eax = new_fd->fd;
      } else if (dir != NULL && dir_lookup(dir, last_part, &inode) && inode_is_dir(inode)) {
        struct dir* new_dir = dir_open(inode);
        struct fd_row* new_fd = malloc(sizeof(struct fd_row));
        new_fd->fd = current_thread->next_aval_fd++;
        new_fd->open_file_object = NULL;
        new_fd->is_dir = true;
        new_fd->dir_or_file = (void*)new_dir;
        list_push_back(&current_thread->fdt, &new_fd->elem);

        f->eax = new_fd->fd;
      } else {
        struct file* curr_file = filesys_open(last_part, dir);
        if (!curr_file) {
          f->eax = -1;
        } else {
          struct fd_row* new_fd = malloc(sizeof(struct fd_row));
          new_fd->fd = current_thread->next_aval_fd++;
          new_fd->open_file_object = curr_file;
          new_fd->is_dir = false;
          new_fd->dir_or_file = curr_file;
          list_push_back(&current_thread->fdt, &new_fd->elem);
          f->eax = new_fd->fd;
        }
      }
      free(last_part);
    } break;

    case SYS_FILESIZE: {
      int fd = args[1];
      struct thread* current_thread = thread_current();
      struct list* fdt_ptr = &current_thread->fdt;
      struct list_elem* e;
      for (e = list_begin(fdt_ptr); e != list_end(fdt_ptr); e = list_next(e)) {
        struct fd_row* curr_fd = list_entry(e, struct fd_row, elem);
        if (curr_fd->fd == fd) {
          struct file* curr_file_obj = curr_fd->open_file_object;
          if (!curr_file_obj) {
            f->eax = -1;
          } else {
            f->eax = file_length(curr_fd->open_file_object);
          }
        }
      }
<<<<<<< HEAD

=======
>>>>>>> 9a3485af536ad5468069ee127682332d7be91969
    } break;

    case SYS_READ: {
      int fd = args[1];
      check_ptr((void*)args[2], sizeof(char*));
      if (args[3] == 0) {
        f->eax = 0;
      } else if (fd == 0) {
        uint8_t* buffer_input = (uint8_t*)args[2];
        uint8_t i = 0;
        for (; i < args[3]; i++) {
          buffer_input[i] = input_getc();
        }
        f->eax = i;
      } else if (fd < 0 || fd == 1) {
        terminate(-1);
      } else if (fd >= 2) {
        struct thread* current_thread = thread_current();
        struct list* fdt_ptr = &current_thread->fdt;
        struct list_elem* e;
        for (e = list_begin(fdt_ptr); e != list_end(fdt_ptr); e = list_next(e)) {
          struct fd_row* curr_fd = list_entry(e, struct fd_row, elem);
          if (curr_fd->fd == fd) {
            if (curr_fd->is_dir) {
              f->eax = -1;
            } else {
              struct file* curr_file_obj = curr_fd->open_file_object;
              if (!curr_file_obj) {
                f->eax = -1;
              } else {
                f->eax = file_read(curr_file_obj, (void*)args[2], args[3]);
              }
            }
          }
        }
      }
    } break;

    case SYS_WRITE: {
      check_ptr((void*)args[2], sizeof(char*));
      int fd = args[1];
      if (fd < 0 || fd == 0) {
        terminate(-1);
      } else if (args[3] == 0) {
        f->eax = 0;
      } else if (fd == 1) {
        if (args[3] < 200) {
          putbuf((char*)args[2], args[3]);
        } else {
          int count = args[3] / 200;
          int rest = args[3] - count * 200;
          for (int i = 0; i < count; i++) {
            putbuf((char*)args[2], 200);
          }
          putbuf((char*)args[2], rest);
        }
        f->eax = args[3];

      } else if (fd >= 2) {
        struct thread* current_thread = thread_current();
        struct list* fdt_ptr = &current_thread->fdt;
        struct list_elem* e;
        for (e = list_begin(fdt_ptr); e != list_end(fdt_ptr); e = list_next(e)) {
          struct fd_row* curr_fd = list_entry(e, struct fd_row, elem);
          if (curr_fd->fd == fd) {
            if (curr_fd->is_dir) {
              f->eax = -1;
            } else {
              struct file* curr_file_obj = curr_fd->open_file_object;
              if (!curr_file_obj) {
                f->eax = -1;
              } else {
                f->eax = file_write(curr_file_obj, (void*)args[2], args[3]);
              }
            }
          }
        }
      }
    } break;

    case SYS_SEEK: {
      unsigned pos = args[2];
      int fd = args[1];
      struct thread* current_thread = thread_current();
      struct list* fdt_ptr = &current_thread->fdt;
      struct list_elem* e;
      for (e = list_begin(fdt_ptr); e != list_end(fdt_ptr); e = list_next(e)) {
        struct fd_row* curr_fd = list_entry(e, struct fd_row, elem);
        if (curr_fd->fd == fd) {
          if (curr_fd->is_dir) {
            f->eax = -1;
          } else {
            struct file* curr_file_obj = curr_fd->open_file_object;
            if (!curr_file_obj) {
              f->eax = -1;
            } else {
              file_seek(curr_file_obj, pos);
            }
          }
        }
      }
    } break;

    case SYS_TELL: {
      int fd = args[1];
      struct thread* current_thread = thread_current();
      struct list* fdt_ptr = &current_thread->fdt;
      struct list_elem* e;
      for (e = list_begin(fdt_ptr); e != list_end(fdt_ptr); e = list_next(e)) {
        struct fd_row* curr_fd = list_entry(e, struct fd_row, elem);
        if (curr_fd->fd == fd) {
          if (curr_fd->is_dir) {
            f->eax = -1;
          } else {
            struct file* curr_file_obj = curr_fd->open_file_object;
            if (!curr_file_obj) {
              f->eax = -1;
            } else {
              f->eax = file_tell(curr_file_obj);
            }
          }
        }
      }
    } break;

    case SYS_CLOSE: {
      int fd = args[1];
      if (fd <= 0 || fd == 1) {
        terminate(-1);
      } else {
        struct thread* current_thread = thread_current();
        struct list* fdt_ptr = &current_thread->fdt;
        struct list_elem* e;
        for (e = list_begin(fdt_ptr); e != list_end(fdt_ptr); e = list_next(e)) {
          struct fd_row* curr_fd = list_entry(e, struct fd_row, elem);
          if (curr_fd->fd == fd) {
            if (curr_fd->is_dir) {
              dir_close((struct dir*)curr_fd->dir_or_file);
              list_remove(e);
            } else {
              struct file* curr_file_obj = curr_fd->open_file_object;
              if (!curr_file_obj) {
                f->eax = -1;
              } else {
                file_close(curr_file_obj);
                list_remove(e);
              }
            }
          }
        }
      }
    } break;

    case SYS_HIT: {
      f->eax = get_buffer_hit();
    } break;

    case SYS_BUFACC: {
      f->eax = get_buffer_ac();
    } break;

    case SYS_ISDIR: {
      int fd = args[1];
      struct thread* current_thread = thread_current();
      struct list* fdt_ptr = &current_thread->fdt;
      struct list_elem* e;
      for (e = list_begin(fdt_ptr); e != list_end(fdt_ptr); e = list_next(e)) {
        struct fd_row* curr_fd = list_entry(e, struct fd_row, elem);
        if (curr_fd->fd == fd) {
          f->eax = curr_fd->is_dir;
        }
      }
    } break;

    case SYS_READDIR: {
      char* name = (char*)args[2];
      check_ptr((void*)args[2], sizeof(char*));
      int fd = args[1];
      if (fd <= 1) {
        terminate(-1);

      } else {
        struct thread* current_thread = thread_current();
        struct list* fdt_ptr = &current_thread->fdt;
        struct list_elem* e;
        for (e = list_begin(fdt_ptr); e != list_end(fdt_ptr); e = list_next(e)) {
          struct fd_row* curr_fd = list_entry(e, struct fd_row, elem);
          if (curr_fd->fd == fd) {
            if (!curr_fd->is_dir) {
              f->eax = false;
            } else {
              struct dir* dir = (struct dir*)curr_fd->dir_or_file;
              f->eax = dir_readdir(dir, name);
            }
          }
        }
      }
    } break;

    case SYS_INUMBER: {
      int fd = args[1];
      if (fd <= 1) {
        terminate(-1);
      } else {
        struct thread* current_thread = thread_current();
        struct list* fdt_ptr = &current_thread->fdt;
        struct list_elem* e;
        for (e = list_begin(fdt_ptr); e != list_end(fdt_ptr); e = list_next(e)) {
          struct fd_row* curr_fd = list_entry(e, struct fd_row, elem);
          if (curr_fd->fd == fd) {
            if (curr_fd->is_dir) {
              struct dir* dir = (struct dir*)(curr_fd->dir_or_file);
              f->eax = dir->inode->sector;
            } else {
              struct file* file = (struct file*)(curr_fd->dir_or_file);
              f->eax = file->inode->sector;
            }
          }
        }
      }
    } break;

    case SYS_MKDIR: {
      check_ptr((void*)args[1], sizeof(const char*));
      const char* dir = (char*)args[1];
      f->eax = filesys_create(dir, 0, true);
    } break;

    case SYS_CHDIR: {
      check_ptr((void*)args[1], sizeof(const char*));
      const char* pathway = (char*)args[1];
      struct inode* inode;
      char* last_part = malloc(NAME_MAX + 1);
      struct dir* last_dir = get_dir_path(pathway, last_part);
      if (last_dir != NULL && dir_lookup(last_dir, last_part, &inode) && inode_is_dir(inode)) {
        dir_close(thread_current()->cwd);
        thread_current()->cwd = dir_open(inode);
        free(last_part);
        f->eax = true;
      } else {
        dir_close(last_dir);
        free(last_part);
        f->eax = false;
      }
    } break;
  }
}

/* Helper function for exiting current thread. */
void terminate(int exit_code) {
  thread_current()->wait_info->exit_code = exit_code;
  printf("%s: exit(%d)\n", thread_current()->name, exit_code);
  thread_exit();
}
